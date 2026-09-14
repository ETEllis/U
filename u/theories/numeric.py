"""Explicit finite numerical realizations, separate from symbolic programs."""
from dataclasses import dataclass
import itertools
import math
import random
from .base import RuntimeFault, UnsupportedOperation, natural, index


def shape(array):
    if not isinstance(array, list):
        if type(array) not in (float, int):
            raise RuntimeFault("ARRAY_ELEMENT", "Strict array realization requires real numeric elements")
        return ()
    if not array:
        return (0,)
    inner = shape(array[0])
    if any(shape(item) != inner for item in array[1:]):
        raise RuntimeFault("ARRAY_RAGGED", "Array must have a rectangular shape")
    return (len(array),) + inner


def _at(array, coordinates):
    for coordinate in coordinates:
        array = array[coordinate]
    return array


@dataclass(frozen=True)
class PVar:
    index: int


@dataclass(frozen=True)
class Normal:
    mean: object
    sigma: object


@dataclass(frozen=True)
class Measure:
    atoms: tuple


@dataclass(frozen=True)
class Model:
    bindings: tuple
    observations: tuple
    output: object


@dataclass
class QReg:
    qubits: int
    amplitudes: tuple
    live: bool = True

    def consume(self):
        if not self.live:
            raise RuntimeFault("RESOURCE_CONSUMED", "Quantum register handle has already been consumed")
        self.live = False


@dataclass(frozen=True)
class QuantumProgram:
    region: object


@dataclass(frozen=True)
class Instrument:
    branches: tuple
    realization: str = "u.reference.statevector/0.1"


@dataclass(frozen=True)
class Trajectory:
    name: str


@dataclass(frozen=True)
class DExpr:
    kind: str
    arguments: tuple


@dataclass(frozen=True)
class DAE:
    variables: tuple
    equations: tuple
    initial: tuple


def _linear(expression, time):
    """Extract affine DAE coefficients; nonlinear/unknown forms fail closed."""
    if type(expression) in (int,float):
        return {}, float(expression)
    if isinstance(expression, Trajectory):
        return {(expression.name, 0): 1.0}, 0.0
    if callable(expression):
        return {}, float(expression(time))
    if not isinstance(expression, DExpr):
        raise UnsupportedOperation("dynamics.solve", "Only explicitly realized affine real trajectories are admitted")
    if expression.kind == "derivative":
        variable = expression.arguments[0]
        if not isinstance(variable, Trajectory):
            raise UnsupportedOperation("dynamics.derivative", "Numerical realization accepts first derivatives of bound trajectories")
        return {(variable.name, 1): 1.0}, 0.0
    if expression.kind in ("add", "equal"):
        left, lconst = _linear(expression.arguments[0], time)
        right, rconst = _linear(expression.arguments[1], time)
        sign = 1.0 if expression.kind == "add" else -1.0
        for key, value in right.items():
            left[key] = left.get(key, 0.0) + sign * value
        return left, lconst + sign * rconst
    if expression.kind == "scale":
        scalar, inner = expression.arguments
        if type(scalar) not in (int,float) or not math.isfinite(scalar):
            raise RuntimeFault("DAE_SCALAR", "Affine coefficients require finite real scalars")
        values, constant = _linear(inner,time)
        return {key: scalar*value for key,value in values.items()}, scalar*constant
    raise UnsupportedOperation("dynamics." + expression.kind)


def _solve_linear(matrix, rhs, tolerance=1e-12):
    """Partial-pivot elimination for square finite systems with singular refusal."""
    size = len(rhs)
    augmented = [list(row)+[rhs[i]] for i,row in enumerate(matrix)]
    for column in range(size):
        pivot = max(range(column,size), key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) <= tolerance:
            raise RuntimeFault("DAE_SINGULAR", "DAE step is singular under the declared pivot tolerance")
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        scale = augmented[column][column]
        augmented[column] = [value/scale for value in augmented[column]]
        for row in range(size):
            if row == column:
                continue
            factor = augmented[row][column]
            augmented[row] = [a-factor*b for a,b in zip(augmented[row],augmented[column])]
    return [row[-1] for row in augmented]


def register(evaluator):
    def contract(a, b, axes_a, axes_b, profile):
        if profile != "f64.strict_left_fold":
            raise UnsupportedOperation("array.contract", "Only explicit strict binary64 left-fold contraction is implemented")
        sa, sb = shape(a), shape(b)
        if len(axes_a) != len(axes_b) or len(set(axes_a)) != len(axes_a) or len(set(axes_b)) != len(axes_b):
            raise RuntimeFault("CONTRACTION_AXES", "Contraction requires paired unique axes")
        for aa,bb in zip(axes_a,axes_b):
            index(aa,len(sa)); index(bb,len(sb))
            if sa[aa] != sb[bb]:
                raise RuntimeFault("CONTRACTION_SHAPE", "Contracted dimensions differ")
        free_a = [axis for axis in range(len(sa)) if axis not in axes_a]
        free_b = [axis for axis in range(len(sb)) if axis not in axes_b]
        result_shape = tuple(sa[axis] for axis in free_a) + tuple(sb[axis] for axis in free_b)
        reduction_shape = [sa[axis] for axis in axes_a]

        def element(coordinates):
            ca, cb = [0]*len(sa), [0]*len(sb)
            for axis,value in zip(free_a,coordinates):
                ca[axis] = value
            for axis,value in zip(free_b,coordinates[len(free_a):]):
                cb[axis] = value
            total = 0.0
            for reduced in itertools.product(*(range(length) for length in reduction_shape)):
                evaluator.tick()
                for aa,bb,value in zip(axes_a,axes_b,reduced):
                    ca[aa], cb[bb] = value,value
                product = float(_at(a,ca)) * float(_at(b,cb))
                total = total + product
            return total

        def build(dimensions, prefix=()):
            return element(prefix) if not dimensions else [build(dimensions[1:],prefix+(i,)) for i in range(dimensions[0])]
        return build(result_shape)

    model_stack = []

    def normal(mean, sigma):
        if not isinstance(mean, PVar) and (type(mean) not in (float,int) or not math.isfinite(mean)):
            raise RuntimeFault("NORMAL_MEAN", "Normal location must be a finite real or bound latent")
        if not isinstance(sigma,PVar) and (type(sigma) not in (float,int) or not math.isfinite(sigma) or sigma <= 0):
            raise RuntimeFault("NORMAL_SCALE", "Normal scale must be positive and finite")
        return Normal(mean,sigma)

    def model(region):
        context = {"bindings": [], "observations": []}
        model_stack.append(context)
        try:
            output = evaluator.invoke(region,[])
        finally:
            model_stack.pop()
        return Model(tuple(context["bindings"]),tuple(context["observations"]),output)

    def sample(distribution):
        if not model_stack:
            raise RuntimeFault("MEASURE_REGION_REQUIRED", "sample binds a measure only inside a model region")
        if not isinstance(distribution,Normal):
            raise UnsupportedOperation("prob.sample", "This model builder admits normal bindings")
        context = model_stack[-1]
        result = PVar(len(context["bindings"]))
        context["bindings"].append(distribution)
        return result

    def observe_all(distribution, data):
        if not model_stack:
            raise RuntimeFault("MEASURE_REGION_REQUIRED", "Observation must belong to a model region")
        if not isinstance(distribution,Normal) or any(type(value) not in (int,float) or not math.isfinite(value) for value in data):
            raise RuntimeFault("OBSERVATION_DOMAIN", "Normal observations require finite real data")
        model_stack[-1]["observations"].append((distribution,tuple(data)))
        return None

    def infer(measure, algorithm, options):
        if not isinstance(measure,Model) or algorithm != "hmc":
            raise UnsupportedOperation("prob.infer", "Reference inference implements finite normal models with HMC")
        seed, chains, draws = natural(options["seed"]), natural(options.get("chains",1)), natural(options.get("draws",1000))
        warmup = natural(options.get("warmup",500))
        steps = natural(options.get("leapfrog_steps",10))
        step_size = float(options.get("step_size",0.1))
        if not chains or not draws or not steps or not math.isfinite(step_size) or step_size <= 0 or not measure.bindings:
            raise RuntimeFault("HMC_OPTIONS", "HMC requires positive chains, draws, leapfrog steps, step size, and a latent")
        rng = random.Random(seed)

        def resolve(value, state):
            return state[value.index] if isinstance(value,PVar) else float(value)

        def log_normal(distribution, value, state):
            mean, sigma = resolve(distribution.mean,state), resolve(distribution.sigma,state)
            if sigma <= 0 or not math.isfinite(sigma):
                return -math.inf
            return -0.5*((value-mean)/sigma)**2 - math.log(sigma) - 0.5*math.log(2*math.pi)

        def log_density(state):
            total = sum(log_normal(distribution,state[i],state) for i,distribution in enumerate(measure.bindings))
            for distribution, data in measure.observations:
                total += sum(log_normal(distribution,value,state) for value in data)
            return total

        def gradient(state):
            result = []
            for i,value in enumerate(state):
                delta = 1e-5*max(1.0,abs(value))
                left,right = list(state),list(state)
                left[i],right[i] = value-delta,value+delta
                result.append((log_density(right)-log_density(left))/(2*delta))
            return result

        samples, accepted, divergent = [],0,0
        for chain in range(chains):
            state = [rng.gauss(0,1) for _ in measure.bindings]
            chain_samples = []
            for iteration in range(warmup+draws):
                momentum = [rng.gauss(0,1) for _ in state]
                proposal, p = list(state),list(momentum)
                grad = gradient(proposal)
                p = [value+0.5*step_size*g for value,g in zip(p,grad)]
                for leap in range(steps):
                    evaluator.tick()
                    proposal = [q+step_size*v for q,v in zip(proposal,p)]
                    grad = gradient(proposal)
                    multiplier = 0.5 if leap == steps-1 else 1.0
                    p = [value+multiplier*step_size*g for value,g in zip(p,grad)]
                energy = log_density(proposal)-log_density(state)+0.5*(sum(v*v for v in momentum)-sum(v*v for v in p))
                if not math.isfinite(energy) or abs(energy)>1000:
                    divergent += 1
                if math.isfinite(energy) and math.log(max(rng.random(),1e-300)) < min(0.0,energy):
                    state,accepted = proposal,accepted+1
                if iteration >= warmup:
                    chain_samples.append(resolve(measure.output,state))
            samples.append(chain_samples)
        return {"status": "approximate", "algorithm": "hmc", "realization": "binary64.central_difference_gradient",
                "seed": seed, "chains": samples, "draws_per_chain": draws, "warmup": warmup,
                "diagnostics": {"acceptance_rate": accepted/(chains*(warmup+draws)), "divergences": divergent,
                                "convergence_claim": False}, "step_size": step_size, "leapfrog_steps": steps}

    def weight(factor, measure):
        if not isinstance(measure,Measure) or not math.isfinite(factor) or factor < 0:
            raise RuntimeFault("MEASURE_WEIGHT", "Finite nonnegative weights and finite measures required")
        return Measure(tuple((value,mass*factor) for value,mass in measure.atoms))

    def normalize(measure):
        if not isinstance(measure,Measure):
            raise UnsupportedOperation("prob.normalize", "Exact normalization is implemented only for finite atomic measures")
        mass = sum(weight for _,weight in measure.atoms)
        if mass <= 0 or not math.isfinite(mass):
            return {"status": "held", "reason": "mass_not_positive_finite", "mass": mass}
        return Measure(tuple((value,weight/mass) for value,weight in measure.atoms))

    def bind(measure, region):
        if not isinstance(measure,Measure):
            raise UnsupportedOperation("prob.bind", "This reference bind realizes finite atomic measures")
        atoms = []
        for value,mass in measure.atoms:
            result = evaluator.invoke(region,[value])
            if not isinstance(result,Measure):
                raise RuntimeFault("TYPE_MEASURE", "Measure bind region must return a measure")
            atoms.extend((value,mass*weight) for value,weight in result.atoms)
        return Measure(tuple(atoms))

    def zero(qubits):
        natural(qubits)
        if qubits > 20:
            raise RuntimeFault("SIMULATOR_LIMIT", "Reference statevector is limited to 20 qubits")
        evaluator.tick(1 << qubits)
        return QReg(qubits,(1+0j,)+(0j,)*((1 << qubits)-1))

    def h(register, target):
        if not isinstance(register,QReg):
            raise RuntimeFault("TYPE_QREG", "Gate requires a joint quantum register")
        index(target,register.qubits)
        register.consume()
        result = list(register.amplitudes)
        mask,scale = 1 << target,1/math.sqrt(2)
        for basis in range(len(result)):
            evaluator.tick()
            if not basis & mask:
                a,b = register.amplitudes[basis],register.amplitudes[basis|mask]
                result[basis],result[basis|mask] = (a+b)*scale,(a-b)*scale
        return QReg(register.qubits,tuple(result))

    def cx(register, control, target):
        if not isinstance(register,QReg):
            raise RuntimeFault("TYPE_QREG", "Gate requires a joint quantum register")
        index(control,register.qubits); index(target,register.qubits)
        if control == target:
            raise RuntimeFault("QUANTUM_WIRES", "Control and target must differ")
        register.consume()
        result = [0j]*len(register.amplitudes)
        for basis,amplitude in enumerate(register.amplitudes):
            evaluator.tick()
            destination = basis ^ (1 << target) if basis & (1 << control) else basis
            result[destination] = amplitude
        return QReg(register.qubits,tuple(result))

    def matrix_value(matrix, dimension):
        if not isinstance(matrix,list) or len(matrix) != dimension or any(not isinstance(row,list) or len(row) != dimension for row in matrix):
            raise RuntimeFault("QUANTUM_MATRIX_SHAPE", "Operator matrix must act on the whole joint register")
        result = []
        for row in matrix:
            converted = [complex(value) for value in row]
            if any(not math.isfinite(value.real) or not math.isfinite(value.imag) for value in converted):
                raise RuntimeFault("QUANTUM_MATRIX_FINITE", "Quantum matrix entries must be finite")
            result.append(converted)
        return result

    def validate_completeness(matrices,dimension,tolerance):
        if type(tolerance) not in (int,float) or not 0 < tolerance <= 1e-8:
            raise RuntimeFault("QUANTUM_TOLERANCE", "Reference validation tolerance must lie in (0, 1e-8]")
        residual = 0.0
        for a in range(dimension):
            for b in range(dimension):
                evaluator.tick()
                entry = sum(matrix[k][a].conjugate()*matrix[k][b] for matrix in matrices for k in range(dimension))
                residual = max(residual,abs(entry-(1.0 if a == b else 0.0)))
        if residual > tolerance:
            raise RuntimeFault("QUANTUM_TRACE_CONTRACT", "Matrices fail the declared completeness tolerance", {"residual":residual,"tolerance":tolerance})
        return residual

    def matvec(matrix,vector):
        result = []
        for row in matrix:
            evaluator.tick(len(vector))
            result.append(sum(a*b for a,b in zip(row,vector)))
        return tuple(result)

    def unitary(register,matrix,tolerance=1e-10):
        if not isinstance(register,QReg) or not register.live:
            raise RuntimeFault("RESOURCE_CONSUMED", "Unitary requires a live joint register")
        dimension = 1 << register.qubits
        matrix = matrix_value(matrix,dimension)
        validate_completeness([matrix],dimension,tolerance)
        result = matvec(matrix,register.amplitudes)
        register.consume()
        return QReg(register.qubits,result)

    def instrument(register,operators,tolerance=1e-10):
        if not isinstance(register,QReg) or not register.live:
            raise RuntimeFault("RESOURCE_CONSUMED", "Instrument requires a live joint register")
        if not isinstance(operators,dict) or not operators:
            raise RuntimeFault("QUANTUM_INSTRUMENT", "Reference instrument requires a nonempty outcome-to-Kraus map")
        dimension = 1 << register.qubits
        matrices = {outcome:matrix_value(matrix,dimension) for outcome,matrix in operators.items()}
        residual = validate_completeness(list(matrices.values()),dimension,tolerance)
        branches = []
        for outcome,matrix in matrices.items():
            amplitudes = matvec(matrix,register.amplitudes)
            probability = sum(abs(value)**2 for value in amplitudes)
            if probability > 0:
                branches.append({"outcome":outcome,"probability":probability,
                                 "post_state":tuple(value/math.sqrt(probability) for value in amplitudes),
                                 "post_state_policy":"simulator_conditional_statevector",
                                 "completeness_residual":residual})
        register.consume()
        return Instrument(tuple(branches))

    def measure_all(register):
        if not isinstance(register,QReg):
            raise RuntimeFault("TYPE_QREG", "Measurement requires a joint register")
        register.consume()
        branches = []
        for basis,amplitude in enumerate(register.amplitudes):
            probability = abs(amplitude)**2
            if probability > 0:
                branches.append({"outcome": basis, "bits": format(basis,f"0{register.qubits}b"),
                                 "probability": probability, "post_state_policy": "discard"})
        return Instrument(tuple(branches))

    def simulate(program):
        if not isinstance(program,QuantumProgram):
            raise RuntimeFault("TYPE_QUANTUM_PROGRAM", "Explicit simulator requires a quantum program")
        result = evaluator.invoke(program.region,[])
        if not isinstance(result,Instrument):
            raise RuntimeFault("QUANTUM_RESULT", "Simulation must end in an explicit instrument")
        return result

    def dae(region):
        variables = tuple(Trajectory(evaluator.fresh(parameter["name"])) for parameter in region.params)
        description = evaluator.invoke(region,list(variables))
        equations,initial = tuple(description["equations"]),tuple(description.get("initial",[]))
        if any(not isinstance(equation,DExpr) or equation.kind != "equal" for equation in equations):
            raise RuntimeFault("DAE_EQUATIONS", "DAE equations require equality expressions")
        return DAE(variables,equations,initial)

    def solve_dae(system, method, options):
        if not isinstance(system,DAE) or method != "backward_euler.affine_index1":
            raise UnsupportedOperation("dynamics.solve", "Only the explicit affine index-1 backward Euler realization is implemented")
        dt,start,steps = float(options["dt"]),float(options.get("start",0.0)),natural(options["steps"])
        if not math.isfinite(dt) or dt <= 0 or not math.isfinite(start):
            raise RuntimeFault("DAE_OPTIONS", "DAE simulation requires finite start and positive finite dt")
        names = [variable.name for variable in system.variables]
        if len(system.equations) != len(names):
            raise RuntimeFault("DAE_STRUCTURE", "Affine index-1 realization requires a square equation system")
        coefficients = [_linear(equation,start) for equation in system.equations]
        derivative_names = {name for mapping,_ in coefficients for name,order in mapping if order == 1 and mapping[(name,order)] != 0}
        state = {}
        for condition in system.initial:
            if not isinstance(condition,DExpr) or condition.kind != "at":
                raise RuntimeFault("DAE_INITIAL", "Unsupported initial condition")
            trajectory,time,value = condition.arguments
            if not isinstance(trajectory,Trajectory) or float(time) != start or trajectory.name in state:
                raise RuntimeFault("DAE_INITIAL", "Initial conditions must uniquely bind declared trajectories at start")
            state[trajectory.name] = float(value)
        if not derivative_names <= state.keys():
            raise RuntimeFault("DAE_INITIAL", "Every differentiated trajectory needs an initial value")
        # Initial algebraic variables are determined from the original equations
        # together with differential initial values, not guessed as zero.
        free = [(name,1) for name in names if name in derivative_names] + [(name,0) for name in names if name not in state]
        if len(free) != len(names):
            raise RuntimeFault("DAE_STRUCTURE", "Initial constraints do not determine an index-1 state")
        matrix,rhs = [],[]
        for mapping,constant in coefficients:
            matrix.append([mapping.get(key,0.0) for key in free])
            rhs.append(-constant-sum(mapping.get((name,0),0.0)*value for name,value in state.items()))
        initial_solution = _solve_linear(matrix,rhs)
        for (name,order),value in zip(free,initial_solution):
            if order == 0:
                state[name] = value
        trace = [{"time": start,"state": dict(state)}]
        residuals = []
        for step in range(steps):
            evaluator.tick()
            time = start+(step+1)*dt
            matrix,rhs = [],[]
            coefficients = [_linear(equation,time) for equation in system.equations]
            for mapping,constant in coefficients:
                matrix.append([mapping.get((name,0),0.0)+mapping.get((name,1),0.0)/dt for name in names])
                rhs.append(-constant+sum(mapping.get((name,1),0.0)*state[name]/dt for name in names))
            values = _solve_linear(matrix,rhs)
            residuals.append(max(abs(sum(a*b for a,b in zip(row,values))-value) for row,value in zip(matrix,rhs)))
            state = dict(zip(names,values))
            trace.append({"time": time,"state": dict(state)})
        return {"status": "approximate", "method": method, "dt": dt, "trace": trace,
                "max_step_residual": max(residuals,default=0.0), "global_error_bound": None,
                "realization": "binary64.affine_backward_euler", "physical": False}

    operations = {
        "array.contract": contract, "dist.normal": normal,
        "prob.model": model, "prob.sample": sample, "prob.observe_all": observe_all, "prob.infer": infer,
        "prob.dirac": lambda value: Measure(((value,1.0),)), "prob.weight": weight,
        "prob.bind": bind, "prob.normalize": normalize,
        "quantum.zero": zero, "quantum.h": h, "quantum.cx": cx, "quantum.measure_all": measure_all,
        "quantum.unitary":unitary,"quantum.instrument":instrument,
        "quantum.program": lambda region: QuantumProgram(region), "quantum.simulate": simulate,
        "dynamics.dae": dae, "dynamics.add": lambda a,b: DExpr("add",(a,b)),
        "dynamics.scale": lambda a,b: DExpr("scale",(a,b)),
        "dynamics.equal": lambda a,b: DExpr("equal",(a,b)),
        "dynamics.derivative": lambda value: DExpr("derivative",(value,)),
        "dynamics.at": lambda value,time,initial: DExpr("at",(value,time,initial)),
        "dynamics.solve": solve_dae,
    }
    for name,operation in operations.items():
        evaluator.register(name,operation)
