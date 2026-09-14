"use strict";

// This is an explanatory finite contract model, not the U compiler, runtime,
// proof checker, or an issuer of authority-bearing receipts.
const constructors = {
  wire: ["A ⊗ B → B ⊗ A", "Connect what already exists.", "Forward or permute existing typed ports. Wiring preserves the interface's resource account; copying, discarding, casting and observing require their own admitted operations.", "A wire does not duplicate a quantum register or an owned allocation."],
  gen: ["gen [ T, operation ]", "Give meaning an inspectable home.", "Apply a generator resolved by its immutable signature. Its theory, model, law premises, direct rule or checked expansion, observables and trust dependencies remain visible.", "Putting a foreign interpreter behind one node does not earn native semantic possession."],
  seq: ["A → B → C", "Connect compatible boundaries.", "Compose in order under one admitted profile, or through an explicit checked adapter. The interface includes resources, effects, clocks and observations, not only carrier names.", "A union of effect names does not prove that two theories compose."],
  par: ["(A → B) ⊗ (C → D)", "Earn the right to stand alongside.", "Compose branches after checking a resource split and the profile's independence obligations. Shared reads require a witness; overlapping writers or duplicated linear ownership fail.", "A lawful split alone does not assert physical simultaneity or arbitrary commutation."],
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
  disjoint: {a:"write α", b:"write β", ownedA:["α"], ownedB:["β"], readsA:[], readsB:[], writesA:["α"], writesB:["β"]},
  "shared-read": {a:"read α", b:"read α", ownedA:[], ownedB:[], readsA:["α"], readsB:["α"], writesA:[], writesB:[]},
  "read-write": {a:"read α", b:"write α", ownedA:[], ownedB:["α"], readsA:["α"], readsB:[], writesA:[], writesB:["α"]},
  duplicate: {a:"own token α", b:"own token α", ownedA:["α"], ownedB:["α"], readsA:[], readsB:[], writesA:[], writesB:[]}
};
const overlaps = (a,b) => a.some(value => b.includes(value));
function inspectArrangement(value, allowsSharing, admitsProfile) {
  if (overlaps(value.ownedA,value.ownedB)) return ["rejected","Rejected: ownership is duplicated","Both branches claim the same linear resource. Neither a profile nor a read witness can grant two simultaneous owners."];
  if (overlaps(value.writesA,[...value.readsB,...value.writesB]) || overlaps(value.writesB,[...value.readsA,...value.writesA])) return ["rejected","Rejected: access conflicts","One branch writes a resource that the other branch reads or writes. The conservative independence rule rejects this overlap."];
  if (overlaps(value.readsA,value.readsB) && !allowsSharing) return ["held","Held: sharing witness required","The reads could be compatible, but the explicit shared-read permission has not been supplied."];
  if (!admitsProfile) return ["held","Held: composition is unadmitted","The declared resource split passes, but the composite semantic profile is missing. Resource compatibility alone does not establish lawful theory composition."];
  return ["accepted","Admissible in this model",overlaps(value.readsA,value.readsB) ? "Both branches only read the shared resource, an explicit read witness is present, and the composite profile is admitted." : "The branches own distinct resources. The profile is admitted, so this declared footprint passes the independence rule."];
}
function updateAdmission() {
  const arrangement = arrangements[scenario.value];
  const [status,title,reason] = inspectArrangement(arrangement,sharing.checked,profile.checked);
  document.getElementById("branch-a").textContent = arrangement.a;
  document.getElementById("branch-b").textContent = arrangement.b;
  const verdict = document.getElementById("admission-status");
  verdict.className = `verdict ${status}`;
  verdict.textContent = title;
  document.getElementById("admission-reason").textContent = reason;
  document.getElementById("admission-earned").textContent = status === "accepted" ? "A lawful declared resource split. Physical simultaneity, general commutation and execution still require their own evidence." : "No composite execution is authorized by this explanatory result. The listed obligation or conflict must be resolved in the actual checked graph.";
}
for (const control of [scenario,sharing,profile]) control.addEventListener("change",updateAdmission);
updateAdmission();
