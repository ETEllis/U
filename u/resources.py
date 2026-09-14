"""Checked reference ownership and authority. Nominal identity is never content."""

from dataclasses import dataclass
import secrets
import math


@dataclass(frozen=True)
class Access:
    allocation: str
    epoch: int
    start: int
    end: int
    mode: str


def independence(left: list[Access], right: list[Access], *, causal_edges=()):
    for access in left + right:
        if (not isinstance(access, Access) or not isinstance(access.allocation, str) or not access.allocation
            or type(access.epoch) is not int or access.epoch < 0
            or type(access.start) is not int or type(access.end) is not int
            or not 0 <= access.start <= access.end
            or access.mode not in {"owned", "read", "write", "shared-read", "mutable"}):
            return {"admitted": False, "reason": "invalid-access-footprint"}
    if causal_edges:
        return {"admitted": False, "reason": "cross-branch-causal-dependence"}
    for a in left:
        for b in right:
            if a.allocation == b.allocation and a.epoch != b.epoch:
                return {"admitted": False, "reason": "incoherent-allocation-epochs"}
            if a.allocation == b.allocation and (a.mode == "owned" or b.mode == "owned"):
                return {"admitted": False, "reason": "duplicated-or-aliased-whole-ownership"}
            overlap = a.allocation == b.allocation and a.epoch == b.epoch and max(a.start, b.start) < min(a.end, b.end)
            if overlap and not (a.mode == b.mode == "shared-read"):
                return {"admitted": False, "reason": "overlapping-exclusive-access"}
    return {"admitted": True, "witness": "range-disjoint-or-shared-read/1"}


@dataclass(frozen=True)
class Handle:
    allocation: str
    epoch: int
    token: str
    mode: str


class Memory:
    def __init__(self):
        self.allocations = {}
        self.tokens = {}

    def alloc(self, size):
        if type(size) is not int or not 0 <= size <= 16_777_216:
            raise ValueError("invalid allocation size")
        name, token = secrets.token_hex(16), secrets.token_hex(16)
        self.allocations[name] = {"bytes": bytearray(size), "initialized": set(), "epoch": 0, "borrow": None}
        handle = Handle(name, 0, token, "owned")
        self.tokens[token] = handle
        return handle

    def _get(self, handle):
        if not isinstance(handle, Handle) or self.tokens.get(handle.token) is not handle:
            raise ValueError("invalid-or-consumed-handle")
        data = self.allocations.get(handle.allocation)
        if data is None or data["epoch"] != handle.epoch:
            raise ValueError("stale-allocation-epoch")
        if data["borrow"] and handle.mode == "owned":
            raise ValueError("owner-suspended-by-borrow")
        return data

    def borrow_mut(self, owner):
        data = self._get(owner)
        if owner.mode != "owned" or data["borrow"]:
            raise ValueError("exclusive-borrow-conflict")
        view = Handle(owner.allocation, owner.epoch, secrets.token_hex(16), "mutable")
        self.tokens[view.token] = view
        data["borrow"] = view.token
        return view

    def return_borrow(self, view):
        data = self._get(view)
        if data["borrow"] != view.token or view.mode != "mutable":
            raise ValueError("not-the-active-borrow")
        del self.tokens[view.token]
        data["borrow"] = None

    def write(self, handle, offset, value):
        data = self._get(handle)
        if type(offset) is not int or not 0 <= offset < len(data["bytes"]):
            raise ValueError("out-of-bounds")
        if type(value) is not int or not 0 <= value <= 255:
            raise ValueError("u8-overflow")
        data["bytes"][offset] = value
        data["initialized"].add(offset)
        return handle

    def read(self, handle, offset):
        data = self._get(handle)
        if offset not in data["initialized"]:
            raise ValueError("uninitialized-read")
        return data["bytes"][offset]

    def free(self, owner):
        data = self._get(owner)
        if owner.mode != "owned" or data["borrow"]:
            raise ValueError("cannot-free-borrow")
        data["epoch"] += 1
        del self.tokens[owner.token]


@dataclass(frozen=True)
class Lease:
    issuer: str
    action: str
    frame: str
    epoch: int
    expires: float
    nonce: str


class Authority:
    """Process-local issuer; cryptographic distributed leases remain separate."""
    def __init__(self, issuer):
        self.issuer, self.epoch, self.issued = issuer, 0, {}

    def issue(self, action, frame, expires):
        if type(expires) not in {int, float} or not math.isfinite(expires):
            raise ValueError("lease-expiry-must-be-finite")
        lease = Lease(self.issuer, action, frame, self.epoch, expires, secrets.token_hex(24))
        self.issued[lease.nonce] = lease
        return lease

    def admit(self, lease, *, action, frame, now):
        if type(now) not in {int, float} or not math.isfinite(now):
            raise ValueError("admission-time-must-be-finite")
        if not isinstance(lease, Lease) or self.issued.get(lease.nonce) is not lease:
            raise ValueError("forged-or-consumed-lease")
        if (lease.issuer, lease.action, lease.frame, lease.epoch) != (self.issuer, action, frame, self.epoch) or now >= lease.expires:
            raise ValueError("lease-scope-or-expiry")
        del self.issued[lease.nonce]
        return True
