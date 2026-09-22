"use strict";

// The finite decision is compiled from model.u to WebAssembly. JavaScript
// presents its result; this instrument does not issue graph-level authority.
const constructors = {
  wire: ["A ⊗ B → B ⊗ A", "Connect what already exists.", "Forward or permute existing typed ports. Wiring preserves the interface's resource account; copying, discarding, casting and observing require their own admitted operations.", "A wire reconnects existing ports while preserving their resource account. It cannot create a second owner."],
  gen: ["gen [ T, operation ]", "Apply an operation with a contract.", "Apply a generator resolved by its immutable signature. Its theory, model, law premises, direct rule or checked expansion, observables and trust dependencies remain visible.", "Native support exposes the operation's rules to U's own analysis and execution."],
  seq: ["A → B → C", "Connect compatible boundaries.", "Compose in order under one admitted profile, or through an explicit checked adapter. The interface includes resources, effects, clocks and observations, not only carrier names.", "A union of effect names does not prove that two theories compose."],
  par: ["(A → B) ⊗ (C → D)", "Compose independent branches.", "Compose branches after checking a resource split and the profile's independence obligations. Shared reads require a witness; overlapping writers or duplicated linear ownership fail.", "A lawful split alone does not assert physical simultaneity or arbitrary commutation."],
  scope: ["ν name . operation", "Introduce a boundary that holds.", "Bind a fresh nominal name or region. Capture avoidance, resource lifetimes and escape restrictions apply to the whole body and its returned interface.", "Lexical scope is not physical allocation, and a closing brace does not prove a reference cannot escape."],
  fix: ["fix [ discipline ]", "State what repetition means.", "Repeat under an admitted discipline: partial computation, monotone relational fixed point, guarded feedback or structural proof recursion. Their premises are distinct.", "Unrestricted computation cannot enter total proof conversion or manufacture a theorem."]
};

const tabs = [...document.querySelectorAll("[data-kernel]")];
function selectKernel(tab, focus = false) {
  for (const item of tabs) {
    const selected = item === tab;
    item.setAttribute("aria-selected", String(selected));
    item.tabIndex = selected ? 0 : -1;
  }
  const [expression, title, description, boundary] = constructors[tab.dataset.kernel];
  document.getElementById("kernel-expression").textContent = expression;
  document.getElementById("kernel-title").textContent = title;
  document.getElementById("kernel-description").textContent = description;
  document.getElementById("kernel-boundary").textContent = boundary;
  document.getElementById("kernel-panel").setAttribute("aria-labelledby", tab.id);
  if (focus) tab.focus();
}
for (const tab of tabs) {
  tab.addEventListener("click", () => selectKernel(tab));
  tab.addEventListener("keydown", event => {
    const index = tabs.indexOf(tab);
    let next;
    if (event.key === "ArrowRight") next = (index + 1) % tabs.length;
    if (event.key === "ArrowLeft") next = (index + tabs.length - 1) % tabs.length;
    if (event.key === "Home") next = 0;
    if (event.key === "End") next = tabs.length - 1;
    if (next !== undefined) { event.preventDefault(); selectKernel(tabs[next], true); }
  });
}

const scenario = document.getElementById("scenario");
const sharing = document.getElementById("sharing");
const profile = document.getElementById("profile");
const arrangements = {
  disjoint: {code:0, a:"write α", b:"write β"},
  "shared-read": {code:1, a:"read α", b:"read α"},
  "read-write": {code:2, a:"read α", b:"write α"},
  duplicate: {code:3, a:"own token α", b:"own token α"}
};
const verdicts = [
  ["rejected","Rejected: ownership is duplicated","Both branches claim the same linear resource. Neither a profile nor a read witness can grant two simultaneous owners."],
  ["rejected","Rejected: access conflicts","One branch writes a resource that the other branch reads or writes. The conservative independence rule rejects this overlap."],
  ["held","Held: sharing witness required","The reads could be compatible, but the explicit shared-read permission has not been supplied."],
  ["held","Held: composition is unadmitted","The declared resource split passes, but the composite semantic profile is missing. Resource compatibility alone does not establish lawful theory composition."],
  ["accepted","Admissible in this model"]
];
const admissionControls = [scenario, sharing, profile];
let nativeDecision = null;
let checkerMessage = "Loading the compiled U check…";
for (const control of admissionControls) control.disabled = true;

function unavailable(message) {
  nativeDecision = null;
  checkerMessage = message;
  for (const control of admissionControls) control.disabled = true;
  document.querySelector(".admission-result").dataset.engine = "unavailable";
  const verdict = document.getElementById("admission-status");
  verdict.className = "verdict held";
  verdict.textContent = checkerMessage;
  document.getElementById("admission-reason").textContent = "No decision is available until the compiled module and its contract load successfully.";
  document.getElementById("admission-earned").textContent = "No result has been established.";
}
function updateAdmission() {
  const arrangement = arrangements[scenario.value];
  if (!arrangement) return unavailable("The selected arrangement is outside this model.");
  document.getElementById("branch-a").textContent = arrangement.a;
  document.getElementById("branch-b").textContent = arrangement.b;
  const verdict = document.getElementById("admission-status");
  if (!nativeDecision) {
    verdict.className = "verdict held";
    verdict.textContent = checkerMessage;
    document.getElementById("admission-reason").textContent = "No decision is available until the compiled module and its contract load successfully.";
    document.getElementById("admission-earned").textContent = "No result has been established.";
    return;
  }
  let code;
  try {
    const result = nativeDecision(arrangement.code, Number(sharing.checked), Number(profile.checked));
    if (typeof result !== "bigint" || result < 0n || result > 4n) throw new Error("Invalid decision result");
    code = Number(result);
  } catch (error) {
    console.error("U check refused an invalid invocation", error);
    return unavailable("The compiled check is unavailable. Reload to try again.");
  }
  const [status, title, detail] = verdicts[code];
  const reason = detail ?? (scenario.value === "shared-read" ?
    "Both branches only read the shared resource, an explicit read witness is present, and the composite profile is admitted." :
    "The branches own distinct resources. The profile is admitted, so this declared footprint passes the independence rule.");
  verdict.className = `verdict ${status}`;
  verdict.textContent = title;
  document.getElementById("admission-reason").textContent = reason;
  document.getElementById("admission-earned").textContent = status === "accepted" ? "A lawful declared resource split. Physical simultaneity, general commutation and execution still require their own evidence." : "No composite execution is authorized by this explanatory result. The listed obligation or conflict must be resolved in the actual checked graph.";
}
for (const control of admissionControls) control.addEventListener("change",updateAdmission);
updateAdmission();

async function loadDecision() {
  const [moduleReply, receiptReply] = await Promise.all([fetch("model.wasm"), fetch("model.wasm.json")]);
  if (!moduleReply.ok || !receiptReply.ok) throw new Error("Compiled assets are unavailable");
  const [binary, receipt] = await Promise.all([moduleReply.arrayBuffer(), receiptReply.json()]);
  const expected = [["scenario", "0", "3"], ["sharing", "0", "1"], ["profile", "0", "1"]];
  if (receipt.contract?.entry !== "decide" || receipt.contract?.return_type !== "Int" ||
      receipt.abi_bounds?.length !== expected.length || expected.some(([name, low, high], i) =>
        receipt.abi_bounds[i].name !== name || receipt.abi_bounds[i].low !== low || receipt.abi_bounds[i].high !== high)) {
    throw new Error("The compiled input contract does not match this instrument");
  }
  const digest = new Uint8Array(await crypto.subtle.digest("SHA-256", binary));
  const hash = [...digest].map(byte => byte.toString(16).padStart(2, "0")).join("");
  if (hash !== receipt.binary_sha256) throw new Error("The compiled module does not match its receipt");
  const {instance} = await WebAssembly.instantiate(binary, {});
  const decide = instance.exports.decide;
  if (typeof decide !== "function" || decide.length !== expected.length) throw new Error("Decision export is unavailable");
  const bounds = receipt.abi_bounds.map(bound => [BigInt(bound.low), BigInt(bound.high)]);
  nativeDecision = (...values) => {
    if (values.length !== bounds.length) throw new RangeError("Wrong number of model inputs");
    const arguments64 = values.map((value, i) => {
      if (!Number.isSafeInteger(value)) throw new RangeError("Exact integer input required");
      const integer = BigInt(value);
      if (integer < bounds[i][0] || integer > bounds[i][1]) throw new RangeError("Input is outside the compiled contract");
      return integer;
    });
    return decide(...arguments64);
  };
  document.querySelector(".admission-result").dataset.engine = "u-wasm";
  for (const control of admissionControls) control.disabled = false;
  updateAdmission();
}
loadDecision().catch(error => {
  console.error("Unable to load the compiled U check", error);
  unavailable("The compiled check could not load. Reload to try again.");
});
