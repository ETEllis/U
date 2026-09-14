"""Runtime values. These representations are stage-0 bootstrap artifacts."""
from dataclasses import dataclass, field
from typing import Any


class RuntimeFault(Exception):
    def __init__(self, code, message, details=None):
        self.code, self.message, self.details = code, message, details or {}
        super().__init__(f"{code}: {message}")

    def as_dict(self):
        return {"status": "error", "code": self.code,
                "message": self.message, "details": self.details}


class UnsupportedOperation(RuntimeFault):
    def __init__(self, operation, reason="No admitted reference realization"):
        super().__init__("UNSUPPORTED_OPERATION", reason, {"operation": operation})


@dataclass(frozen=True)
class TypeSymbol:
    name: str
    arguments: tuple = ()


@dataclass(frozen=True)
class Capability:
    name: str
    _authority: object = field(repr=False, compare=False)


@dataclass(frozen=True)
class Sum:
    tag: str
    value: Any = None


@dataclass(frozen=True)
class Partial:
    status: str
    value: Any = None
    steps: int = 0


@dataclass(frozen=True)
class Option:
    present: bool
    value: Any = None


@dataclass
class OwnedBuffer:
    data: list
    live: bool = True
    borrowed: bool = False
    epoch: int = 0

    def usable(self):
        if not self.live:
            raise RuntimeFault("RESOURCE_CONSUMED", "Owning handle has been consumed")
        if self.borrowed:
            raise RuntimeFault("RESOURCE_BORROWED", "Ownership is suspended during a borrow")


@dataclass
class MutView:
    owner: OwnedBuffer
    epoch: int
    active: bool = True

    def usable(self):
        if not self.active or not self.owner.live or self.owner.epoch != self.epoch:
            raise RuntimeFault("LIFETIME_ENDED", "Mutable view's lifetime has ended")
        if not self.owner.borrowed:
            raise RuntimeFault("INVALID_BORROW", "View does not hold an exclusive borrow")


@dataclass(frozen=True)
class Bits:
    value: int
    width: int

    def __post_init__(self):
        if self.width <= 0 or self.value < 0 or self.value >= 1 << self.width:
            raise RuntimeFault("BITS_RANGE", "Bit value is outside its declared width")


def natural(value):
    if type(value) is not int or value < 0:
        raise RuntimeFault("TYPE_NAT", "Expected a nonnegative arbitrary-precision integer")
    return value


def integer(value):
    if type(value) is not int:
        raise RuntimeFault("TYPE_INT", "Expected an arbitrary-precision integer")
    return value


def boolean(value):
    if type(value) is not bool:
        raise RuntimeFault("TYPE_BOOL", "Expected Bool")
    return value


def index(value, length):
    natural(value)
    if value >= length:
        raise RuntimeFault("INDEX_RANGE", "Index outside resource", {"index": value, "length": length})
    return value
