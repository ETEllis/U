"""Tasks and explicit actor, finite-temporal, clock and GPU realizations."""
from concurrent.futures import ThreadPoolExecutor, CancelledError
from dataclasses import dataclass, field
import threading
import urllib.request
from .base import (RuntimeFault, UnsupportedOperation, Option, OwnedBuffer,
                   MutView, natural, index, boolean)


class Task:
    """A lazy scheduled task with distinct completion/failure/cancellation.

    Cancellation after a side effect starts is a request, never a rollback
    claim. State and outcome remain inspectable independently of result().
    """
    def __init__(self, action):
        self.action, self.future, self.executor = action,None,None
        self.state, self.value, self.error = "pending",None,None
        self.cancellation_requested = False
        self._lock = threading.RLock()

    def start(self):
        with self._lock:
            if self.state == "cancelled":
                return self
            if self.future is None:
                self.state = "running"
                self.executor = ThreadPoolExecutor(max_workers=1,thread_name_prefix="u-task")
                self.future = self.executor.submit(self.action)
                self.future.add_done_callback(self._finished)
        return self

    def _finished(self, future):
        with self._lock:
            try:
                self.value = future.result()
                self.state = "completed"
            except CancelledError:
                self.state = "cancelled"
            except Exception as error:
                self.state,self.error = "failed",error
            self.executor.shutdown(wait=False)

    def cancel(self):
        with self._lock:
            self.cancellation_requested = True
            if self.future is None:
                self.state = "cancelled"
                return True
            return self.future.cancel()

    def result(self, timeout=None):
        self.start()
        if self.state == "cancelled":
            raise RuntimeFault("TASK_CANCELLED", "Task was cancelled before completion")
        try:
            return self.future.result(timeout)
        except CancelledError as error:
            raise RuntimeFault("TASK_CANCELLED", "Task was cancelled before completion") from error

    def outcome(self):
        return {"state": self.state,"value": self.value,
                "error": str(self.error) if self.error else None,
                "cancellation_requested": self.cancellation_requested}


@dataclass(frozen=True)
class ActorStep:
    status: str
    value: object


@dataclass(frozen=True)
class Child:
    name: str
    initial: object
    worker: object


@dataclass
class Supervisor:
    children: dict
    max_restarts: int
    window: float
    states: dict
    restarts: list = field(default_factory=list)
    mailbox: list = field(default_factory=list)
    events: list = field(default_factory=list)
    status: str = "running"
    time: float = 0.0


@dataclass(frozen=True)
class Fairness:
    action: object


@dataclass(frozen=True)
class Behavior:
    initial: object
    action: object
    stuttering: bool
    fairness: tuple


@dataclass(frozen=True)
class Temporal:
    kind: str
    arguments: tuple


@dataclass(eq=False)
class Clock:
    name: str
    tick: int = 0


@dataclass
class InputSignal:
    clock: Clock
    values: list


@dataclass
class Register:
    clock: Clock
    value: object
    update: object


@dataclass(frozen=True)
class GpuRead:
    data: tuple


@dataclass
class GpuElement:
    owner: OwnedBuffer
    offset: int
    active: bool = True
    written: bool = False

    def usable(self):
        if not self.active:
            raise RuntimeFault("GPU_LEASE_EXPIRED", "GPU element lease ended with its lane")


def _strong_components(vertices, adjacency):
    """Iterative reachability partition avoids depending on Python recursion."""
    remaining,components = set(vertices),[]
    reverse = {vertex: set() for vertex in vertices}
    for vertex,targets in adjacency.items():
        for target in targets:
            if target in reverse:
                reverse[target].add(vertex)

    def reach(start, graph):
        seen,pending = {start},[start]
        while pending:
            for target in graph.get(pending.pop(),()):
                if target in remaining and target not in seen:
                    seen.add(target); pending.append(target)
        return seen

    while remaining:
        seed = min(remaining)
        component = reach(seed,adjacency)&reach(seed,reverse)
        remaining -= component
        components.append(component)
    return components


def register(evaluator):
    current_tick = []

    def then(task, continuation):
        if not isinstance(task,Task):
            raise RuntimeFault("TYPE_TASK", "Continuation requires a Task")
        return Task(lambda: evaluator.invoke(continuation,[task.result()]))

    def http_get(capability,url):
        evaluator.require_capability(capability,"network")
        if not isinstance(url,str) or not url.startswith(("https://","http://")):
            raise RuntimeFault("HTTP_URL", "HTTP requires an explicit http(s) URL")
        def request():
            with urllib.request.urlopen(url,timeout=15) as reply:
                payload = reply.read(8*1024*1024+1)
                if len(payload)>8*1024*1024:
                    raise RuntimeFault("HTTP_SIZE", "Response exceeded the 8 MiB reference limit")
                return {"status": reply.status,"body": payload,"charset": reply.headers.get_content_charset() or "utf-8"}
        return Task(request)

    def await_task(task):
        if evaluator.active_borrows:
            raise RuntimeFault("BORROW_ACROSS_AWAIT", "A mutable borrow cannot cross task suspension")
        if not isinstance(task,Task):
            raise RuntimeFault("TYPE_TASK", "await requires a Task")
        return task.result()

    def supervise(capability,strategy,children,options):
        evaluator.require_capability(capability,"actors")
        if strategy != "one_for_one":
            raise UnsupportedOperation("actor.supervise", "Only the one-for-one strategy is implemented")
        named = {child.name: child for child in children}
        if len(named) != len(children):
            raise RuntimeFault("ACTOR_NAMES", "Child names must be unique")
        max_restarts,window = natural(options["max_restarts"]),float(options["window"])
        if window <= 0:
            raise RuntimeFault("ACTOR_WINDOW", "Restart window must be positive")
        return Supervisor(named,max_restarts,window,{name: child.initial for name,child in named.items()})

    def send(supervisor,name,message):
        if not isinstance(supervisor,Supervisor) or supervisor.status != "running" or name not in supervisor.children:
            raise RuntimeFault("ACTOR_TARGET", "Message target is not a running supervised child")
        supervisor.mailbox.append((name,message))
        return None

    def run_actors(supervisor,limit=1000):
        for _ in range(natural(limit)):
            if not supervisor.mailbox or supervisor.status != "running":
                break
            evaluator.tick()
            name,message = supervisor.mailbox.pop(0)
            child = supervisor.children[name]
            step = evaluator.invoke(child.worker,[supervisor.states[name],message])
            if not isinstance(step,ActorStep):
                raise RuntimeFault("ACTOR_STEP", "Actor worker must return an ActorStep")
            if step.status == "continue":
                supervisor.states[name] = step.value
                supervisor.events.append({"kind":"handled","child":name,"state":step.value,"time":supervisor.time})
            elif step.status == "failed":
                supervisor.restarts = [time for time in supervisor.restarts if supervisor.time-time <= supervisor.window]
                supervisor.restarts.append(supervisor.time)
                if len(supervisor.restarts)>supervisor.max_restarts:
                    supervisor.status = "failed"
                    supervisor.events.append({"kind":"intensity_exceeded","child":name,"time":supervisor.time})
                else:
                    supervisor.states[name] = child.initial
                    supervisor.events.append({"kind":"restarted","child":name,"reason":step.value,"time":supervisor.time})
            else:
                raise RuntimeFault("ACTOR_STEP", "Unknown actor transition")
        return {"status":supervisor.status,"states":dict(supervisor.states),"pending":len(supervisor.mailbox),"events":list(supervisor.events),"realization":"deterministic_fifo_simulator"}

    def advance_time(supervisor,amount):
        if amount < 0:
            raise RuntimeFault("ACTOR_TIME", "Simulator time cannot move backwards")
        supervisor.time += float(amount)
        return supervisor

    def temporal_spec(initial,action,stuttering,*fairness):
        if stuttering != "stutter" or any(not isinstance(item,Fairness) for item in fairness):
            raise RuntimeFault("TEMPORAL_SPEC", "Reference spec requires explicit stuttering and named weak fairness")
        return Behavior(initial,action,True,tuple(fairness))

    def temporal_check(system,property,states):
        if not isinstance(system,Behavior) or not isinstance(property,Temporal):
            raise RuntimeFault("TEMPORAL_TYPES", "Expected behavior specification and temporal property")
        if not isinstance(states,list) or any(type(state) not in (int,str,bool) for state in states) or len(set(states)) != len(states):
            raise RuntimeFault("MODEL_STATES", "Finite reference states must be unique scalar values")
        if system.initial not in states:
            raise RuntimeFault("MODEL_INITIAL", "Initial state is outside the declared closed model")
        adjacency, action_edges = {},{}
        for state in states:
            next_state = evaluator.invoke(system.action,[state])
            if not isinstance(next_state,Option):
                raise RuntimeFault("TEMPORAL_ACTION", "Action must return Option(state)")
            if next_state.present and next_state.value not in states:
                raise RuntimeFault("MODEL_NOT_CLOSED", "An enabled transition leaves the supplied state set", {"source":state,"target":next_state.value})
            adjacency[state] = {state}
            action_edges[state] = {next_state.value} if next_state.present else set()
            adjacency[state].update(action_edges[state])
        reachable,pending = {system.initial},[system.initial]
        while pending:
            for target in adjacency[pending.pop()]:
                if target not in reachable:
                    reachable.add(target); pending.append(target)
        if reachable != set(states):
            raise RuntimeFault("MODEL_NOT_REACHABLE", "Declared closed reachable states include unreachable members")
        fairness_edges = []
        for fairness in system.fairness:
            edges = {}
            for state in states:
                result = evaluator.invoke(fairness.action,[state])
                if not isinstance(result,Option):
                    raise RuntimeFault("TEMPORAL_FAIR_ACTION", "Fair action must return Option(state)")
                edges[state] = {result.value} if result.present else set()
                if result.present and result.value not in adjacency[state]:
                    raise RuntimeFault("FAIRNESS_NOT_SUBACTION", "Fairness action is not a transition of the declared system")
            fairness_edges.append(edges)

        def fair_cycle(component):
            # A strongly connected recurrent set has a weakly fair run iff
            # each fair action is disabled somewhere or has an internal edge.
            for edges in fairness_edges:
                if all(edges[state] for state in component) and not any(edges[state]&component for state in component):
                    return False
            return True

        def verify(formula):
            if formula.kind == "and":
                parts = [verify(part) for part in formula.arguments]
                return {"holds": all(part["holds"] for part in parts),"conjuncts":parts}
            region = formula.arguments[0]
            satisfies = {state for state in states if boolean(evaluator.invoke(region,[state]))}
            if formula.kind == "always":
                violating = reachable-satisfies
                return {"holds":not violating,"operator":"always","counterexample_state":next(iter(violating),None)}
            if formula.kind == "eventually":
                bad = reachable-satisfies
                if system.initial not in bad:
                    return {"holds":True,"operator":"eventually"}
                bad_adjacency = {state:adjacency[state]&bad for state in bad}
                bad_reachable,pending = {system.initial},[system.initial]
                while pending:
                    for target in bad_adjacency[pending.pop()]:
                        if target not in bad_reachable:
                            bad_reachable.add(target); pending.append(target)
                for component in _strong_components(bad_reachable,bad_adjacency):
                    cyclic = len(component)>1 or any(state in bad_adjacency[state] for state in component)
                    if cyclic and fair_cycle(component):
                        return {"holds":False,"operator":"eventually","counterexample_recurrent_set":sorted(component)}
                return {"holds":True,"operator":"eventually"}
            raise UnsupportedOperation("temporal."+formula.kind)
        result = verify(property)
        return {"status":"verified" if result["holds"] else "refuted","property":result,
                "states":len(states),"transitions":sum(len(targets) for targets in adjacency.values()),
                "fairness":"weak_action_fairness","realization":"finite_explicit_graph","infinite_state_proof":False}

    def reg(clock,initial,region):
        if not isinstance(clock,Clock):
            raise RuntimeFault("TYPE_CLOCK", "Register requires an explicit clock")
        return Register(clock,initial,region)

    def at_tick(signal):
        if not current_tick:
            raise RuntimeFault("CLOCK_REGION", "Signal observation requires a clock event region")
        clock = current_tick[-1]
        if not isinstance(signal,(InputSignal,Register)) or signal.clock is not clock:
            raise RuntimeFault("CLOCK_MISMATCH", "Observed signal belongs to a different clock")
        if isinstance(signal,Register):
            return signal.value
        return signal.values[index(clock.tick,len(signal.values))]

    def simulate_clock(clock,registers,ticks):
        natural(ticks)
        if any(not isinstance(register,Register) or register.clock is not clock for register in registers):
            raise RuntimeFault("CLOCK_MISMATCH", "All registers must belong to this clock")
        if len({id(register) for register in registers}) != len(registers):
            raise RuntimeFault("REGISTER_ALIAS", "Each register can publish once per event")
        trace = [[register.value for register in registers]]
        for _ in range(ticks):
            evaluator.tick()
            current_tick.append(clock)
            try:
                next_values = [evaluator.invoke(register.update,[register.value]) for register in registers]
            finally:
                current_tick.pop()
            for register,value in zip(registers,next_values):
                register.value = value
            clock.tick += 1
            trace.append(next_values)
        return {"realization":"two_state_positive_edge_simulator","clock":clock.name,"trace":trace,"physical":False}

    def partition_launch(geometry,buffer,region):
        if not isinstance(buffer,OwnedBuffer):
            raise RuntimeFault("GPU_OWNER", "Launch requires a live exclusive output buffer")
        buffer.usable()
        grid,block = geometry["grid"],geometry["block"]
        if len(grid)!=1 or len(block)!=1 or natural(block[0])==0 or natural(grid[0])*block[0]<len(buffer.data):
            raise RuntimeFault("GPU_GEOMETRY", "Reference partition is one-dimensional and must cover the output")
        # Reserve the owner until the task has finished. The input handle is
        # consumed only when the task starts; pending cancellation preserves it.
        buffer.borrowed = True
        def launch():
            if not buffer.live:
                raise RuntimeFault("RESOURCE_CONSUMED", "Output owner was consumed before launch")
            result = OwnedBuffer(list(buffer.data))
            buffer.live,buffer.borrowed = False,False
            for lane in range(grid[0]*block[0]):
                evaluator.tick()
                if lane >= len(result.data):
                    continue
                element = GpuElement(result,lane)
                try:
                    returned = evaluator.invoke(region,[{"global_x":lane,"block_x":lane//block[0],"local_x":lane%block[0]},element])
                    if returned is not element or not element.written:
                        raise RuntimeFault("GPU_LEASE_RETURN", "Lane must return its written exclusive element")
                finally:
                    element.active = False
            return result
        task = Task(launch)
        original_cancel = task.cancel
        def cancel():
            cancelled = original_cancel()
            if cancelled and buffer.live:
                buffer.borrowed = False
            return cancelled
        task.cancel = cancel
        return task

    def gpu_load(buffer,offset):
        if not isinstance(buffer,GpuRead):
            raise RuntimeFault("GPU_READ_LEASE", "GPU load requires an immutable read lease")
        return buffer.data[index(offset,len(buffer.data))]

    def gpu_read(element):
        if not isinstance(element,GpuElement):
            raise RuntimeFault("GPU_ELEMENT", "Lane read requires an exclusive element lease")
        element.usable()
        return element.owner.data[element.offset]

    def gpu_write(element,value):
        if not isinstance(element,GpuElement):
            raise RuntimeFault("GPU_ELEMENT", "Lane write requires an exclusive element lease")
        element.usable()
        element.owner.data[element.offset] = value
        element.written = True
        return element

    operations = {
        "async.then":then,"async.await":await_task,"async.cancel":lambda task:task.cancel(),
        "async.start":lambda task:task.start(), "http.get":http_get,
        "http.body_text":lambda reply:reply["body"].decode(reply["charset"]),
        "duration.seconds":lambda value:float(value),"actor.child":lambda name,initial,worker:Child(name,initial,worker),
        "actor.continue":lambda state:ActorStep("continue",state),"actor.fail":lambda reason:ActorStep("failed",reason),
        "actor.supervise":supervise,"actor.send":send,"actor.run":run_actors,"actor.advance_time":advance_time,
        "temporal.spec":temporal_spec,"temporal.stutter":lambda:"stutter",
        "temporal.weak_fair":lambda action:Fairness(action),
        "temporal.always":lambda region:Temporal("always",(region,)),
        "temporal.eventually":lambda region:Temporal("eventually",(region,)),
        "temporal.and":lambda *parts:Temporal("and",parts),"temporal.check":temporal_check,
        "model.closed_reachable_states":lambda values:values,
        "hw.reg":reg,"hw.at_tick":at_tick,"hw.simulate":simulate_clock,
        "gpu.partition_launch":partition_launch,"gpu.load":gpu_load,"gpu.read":gpu_read,"gpu.write":gpu_write,
    }
    for name,operation in operations.items():
        evaluator.register(name,operation)
