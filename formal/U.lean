import Std

/-!
# U: small structural, resource and evidence invariants

These theorems concern the definitions in this file. They do not establish
correctness of the Python evaluator, floating-point Jacobians, admission
implementation, hashing, native code generation or physical backends.
-/

namespace UFormal

/-- Six structural tags. Numeric interfaces record arity, not a full U type. -/
inductive Graph : Nat → Nat → Type where
  | wire (n : Nat) : Graph n n
  | gen (name : Nat) (inputs outputs : Nat) : Graph inputs outputs
  | seq {a b c : Nat} : Graph a b → Graph b c → Graph a c
  | par {a b c d : Nat} : Graph a b → Graph c d → Graph (a+c) (b+d)
  | scope {a b : Nat} : Graph a b → Graph a b
  | fix {a : Nat} : Graph a a → Graph a a

/-- Ordered generator occurrence projection; not execution or loop unfolding. -/
def generators {a b : Nat} : Graph a b → List Nat
  | .wire _ => []
  | .gen name _ _ => [name]
  | .seq first second => generators first ++ generators second
  | .par left right => generators left ++ generators right
  | .scope body => generators body
  | .fix body => generators body

theorem sequential_occurrences_associative {a b c d : Nat}
    (f : Graph a b) (g : Graph b c) (h : Graph c d) :
    generators (.seq (.seq f g) h) = generators (.seq f (.seq g h)) := by
  have appendAssoc (first second third : List Nat) :
      (first ++ second) ++ third = first ++ (second ++ third) := by
    induction first with
    | nil => rfl
    | cons head tail ih => exact congrArg (List.cons head) ih
  exact appendAssoc (generators f) (generators g) (generators h)

theorem scope_preserves_occurrences {a b : Nat} (g : Graph a b) :
    generators (.scope g) = generators g := rfl

theorem occurrence_order_is_not_automatically_commutative :
    generators (.par (.gen 1 1 1) (.gen 2 1 1)) ≠
      generators (.par (.gen 2 1 1) (.gen 1 1 1)) := by decide

/-- Exact functions are applied in declared source order. -/
def runPath {State : Type} : List (State → State) → State → State
  | [], state => state
  | step :: rest, state => runPath rest (step state)

theorem runPath_append {State : Type} (first second : List (State → State))
    (state : State) :
    runPath (first ++ second) state = runPath second (runPath first state) := by
  induction first generalizing state with
  | nil => rfl
  | cons step rest ih => exact ih (step state)

theorem source_order_composition_associative {State : Type}
    (first second third : List (State → State)) (state : State) :
    runPath ((first ++ second) ++ third) state =
      runPath third (runPath second (runPath first state)) := by
  rw [runPath_append, runPath_append]

theorem reversing_source_order_changes_result :
    runPath [(fun n : Nat => n+1), (fun n => n*2)] 0 ≠
      runPath [(fun n : Nat => n*2), (fun n => n+1)] 0 := by decide

/-! Trit aperture and the ordered prefix barrier. -/

inductive Trit where
  | negative | zero | positive
  deriving DecidableEq, Repr

def tritValue : Trit → Int
  | .negative => -1
  | .zero => 0
  | .positive => 1

def invertTrit : Trit → Trit
  | .negative => .positive
  | .zero => .zero
  | .positive => .negative

def invert : List Trit → List Trit
  | [] => []
  | head :: tail => invertTrit head :: invert tail

theorem invertTrit_involutive (t : Trit) : invertTrit (invertTrit t) = t := by
  cases t <;> rfl

theorem invertTrit_value (t : Trit) : tritValue (invertTrit t) = -tritValue t := by
  cases t <;> decide

theorem invert_involutive (trits : List Trit) : invert (invert trits) = trits := by
  induction trits with
  | nil => rfl
  | cons head tail ih =>
    change invertTrit (invertTrit head) :: invert (invert tail) = head :: tail
    rw [invertTrit_involutive, ih]

/-- Every running prefix, starting from `balance`, stays nonnegative. -/
def PrefixValid : List Trit → Int → Prop
  | [], _ => True
  | head :: tail, balance =>
      0 ≤ balance + tritValue head ∧ PrefixValid tail (balance + tritValue head)

def Admitted (trits : List Trit) : Prop := PrefixValid trits 0

def Aperture : List Trit → Prop
  | [] => True
  | head :: tail => head = .zero ∧ Aperture tail

/-- Both polarities pass from zero exactly on the all-zero aperture. -/
theorem both_polarities_iff_aperture (trits : List Trit) :
    (Admitted trits ∧ Admitted (invert trits)) ↔ Aperture trits := by
  induction trits with
  | nil => exact ⟨fun _ => True.intro, fun _ => ⟨True.intro, True.intro⟩⟩
  | cons head tail ih =>
    cases head with
    | negative =>
      constructor
      · intro both
        have impossible : ¬ ((0 : Int) ≤ -1) := by decide
        exact False.elim (impossible both.1.1)
      · intro aperture
        cases aperture.1
    | zero =>
      constructor
      · intro both
        exact ⟨rfl, ih.mp ⟨both.1.2, both.2.2⟩⟩
      · intro aperture
        have rest := ih.mpr aperture.2
        exact ⟨⟨by decide, rest.1⟩, ⟨by decide, rest.2⟩⟩
    | positive =>
      constructor
      · intro both
        have impossible : ¬ ((0 : Int) ≤ -1) := by decide
        exact False.elim (impossible both.2.1)
      · intro aperture
        cases aperture.1

theorem aperture_fixed_by_inversion (trits : List Trit) (h : Aperture trits) :
    invert trits = trits := by
  induction trits with
  | nil => rfl
  | cons head tail ih =>
    obtain ⟨hz, ht⟩ := h
    cases hz
    change Trit.zero :: invert tail = Trit.zero :: tail
    exact congrArg (List.cons Trit.zero) (ih ht)

theorem accepted_non_aperture_has_rejected_inverse (trits : List Trit)
    (accepted : Admitted trits) (nonzero : ¬ Aperture trits) :
    ¬ Admitted (invert trits) := by
  intro opposite
  exact nonzero ((both_polarities_iff_aperture trits).mp ⟨accepted, opposite⟩)

theorem source_order_prefix_discriminator :
    Admitted [.positive, .negative] ∧ ¬ Admitted [.negative, .positive] := by
  constructor
  · exact ⟨by decide, ⟨by decide, True.intro⟩⟩
  · intro admitted
    have impossible : ¬ ((0 : Int) ≤ -1) := by decide
    exact impossible admitted.1

/-! Resource splitting: nominal exclusive tokens, not content hashes. -/

structure AdmittedSplit (whole left right : List Nat) : Prop where
  partition : whole = left ++ right
  exclusive : whole.Nodup

theorem exclusive_of_nodup_append {Token : Type} (left right : List Token) :
    (left ++ right).Nodup → ∀ token, token ∈ left → token ∈ right → False := by
  induction left with
  | nil =>
    intro _ _ impossible _
    cases impossible
  | cons head tail ih =>
    intro nodup token leftMember rightMember
    cases nodup with
    | cons apart nodupTail =>
      cases leftMember with
      | head => exact apart head (List.mem_append_right tail rightMember) rfl
      | tail _ member => exact ih nodupTail token member rightMember

theorem admitted_split_has_no_shared_exclusive_token
    {whole left right : List Nat} (admission : AdmittedSplit whole left right)
    (token : Nat) : ¬ (token ∈ left ∧ token ∈ right) := by
  intro duplicate
  have nodup : (left ++ right).Nodup := admission.partition ▸ admission.exclusive
  exact exclusive_of_nodup_append left right nodup token duplicate.1 duplicate.2

inductive AccessState where
  | owned
  | borrowed (epoch : Nat)
  | freed
  deriving DecidableEq

def borrow (state : AccessState) (epoch : Nat) : Option AccessState :=
  match state with
  | .owned => some (.borrowed epoch)
  | .borrowed _ => none
  | .freed => none

def matchEpoch (active epoch : Nat) : Option AccessState :=
  match active with
  | 0 =>
    match epoch with
    | 0 => some .owned
    | .succ _ => none
  | .succ previous =>
    match epoch with
    | 0 => none
    | .succ previousEpoch => matchEpoch previous previousEpoch
termination_by structural active

def returnBorrow (state : AccessState) (epoch : Nat) : Option AccessState :=
  match state with
  | .owned => none
  | .borrowed active => matchEpoch active epoch
  | .freed => none

theorem borrow_suspends_owning_access (epoch : Nat) :
    borrow .owned epoch = some (.borrowed epoch) ∧
      borrow (.borrowed epoch) epoch = none := ⟨rfl, rfl⟩

theorem stale_borrow_cannot_restore_owner (active stale : Nat) (h : active ≠ stale) :
    returnBorrow (.borrowed active) stale = none := by
  change matchEpoch active stale = none
  induction active generalizing stale with
  | zero =>
    cases stale with
    | zero => exact False.elim (h rfl)
    | succ _ => rfl
  | succ active ih =>
    cases stale with
    | zero => rfl
    | succ stale => exact ih stale (fun equal => h (congrArg Nat.succ equal))

theorem current_borrow_can_restore_owner (epoch : Nat) :
    returnBorrow (.borrowed epoch) epoch = some .owned := by
  change matchEpoch epoch epoch = some .owned
  induction epoch with
  | zero => rfl
  | succ previous ih => exact ih

/-! Analysis bind: evidence retention and no output manufacture by HOLD. -/

inductive Analysis (Evidence Value : Type) where
  | done (value : Value) (earned : List Evidence)
  | held (reason : String) (earned : List Evidence)

def output {E A : Type} : Analysis E A → Option A
  | .done value _ => some value
  | .held _ _ => none

def earned {E A : Type} : Analysis E A → List E
  | .done _ evidence => evidence
  | .held _ evidence => evidence

def bind {E A B : Type} (before : Analysis E A)
    (continuation : A → Analysis E B) : Analysis E B :=
  match before with
  | .held reason evidence => .held reason evidence
  | .done value prior =>
    match continuation value with
    | .done next newer => .done next (prior ++ newer)
    | .held reason newer => .held reason (prior ++ newer)

theorem held_cannot_manufacture_output {E A B : Type}
    (reason : String) (prior : List E) (continuation : A → Analysis E B) :
    output (bind (.held reason prior) continuation) = none := rfl

theorem held_cannot_add_earned_artifacts {E A B : Type}
    (reason : String) (prior : List E) (continuation : A → Analysis E B) :
    earned (bind (.held reason prior) continuation) = prior := rfl

theorem downstream_hold_preserves_earlier_evidence {E A B : Type}
    (value : A) (prior newer : List E) (reason : String)
    (continuation : A → Analysis E B)
    (held : continuation value = .held reason newer) :
    earned (bind (.done value prior) continuation) = prior ++ newer := by
  change earned (match continuation value with
    | .done next evidence => .done next (prior ++ evidence)
    | .held why evidence => .held why (prior ++ evidence)) = prior ++ newer
  rw [held]
  rfl

theorem earlier_artifact_survives_downstream_hold {E A B : Type}
    (artifact : E) (value : A) (prior newer : List E) (reason : String)
    (continuation : A → Analysis E B)
    (held : continuation value = .held reason newer) (member : artifact ∈ prior) :
    artifact ∈ earned (bind (.done value prior) continuation) := by
  rw [downstream_hold_preserves_earlier_evidence value prior newer reason continuation held]
  exact List.mem_append_left newer member

structure BoundEvidence where
  path : Nat
  payload : Nat

def Compatible (path : Nat) (checks : List BoundEvidence) : Prop :=
  ∀ check ∈ checks, check.path = path

theorem nonempty_compatible_checks_determine_one_path
    (checks : List BoundEvidence) (first second : Nat)
    (nonempty : ∃ check, check ∈ checks)
    (left : Compatible first checks) (right : Compatible second checks) :
    first = second := by
  obtain ⟨check, member⟩ := nonempty
  exact (left check member).symm.trans (right check member)

-- Dependency checks are printed into the verification command's output.
#print axioms sequential_occurrences_associative
#print axioms scope_preserves_occurrences
#print axioms occurrence_order_is_not_automatically_commutative
#print axioms runPath_append
#print axioms source_order_composition_associative
#print axioms reversing_source_order_changes_result
#print axioms invertTrit_involutive
#print axioms invertTrit_value
#print axioms invert_involutive
#print axioms both_polarities_iff_aperture
#print axioms aperture_fixed_by_inversion
#print axioms accepted_non_aperture_has_rejected_inverse
#print axioms source_order_prefix_discriminator
#print axioms exclusive_of_nodup_append
#print axioms admitted_split_has_no_shared_exclusive_token
#print axioms borrow_suspends_owning_access
#print axioms stale_borrow_cannot_restore_owner
#print axioms current_borrow_can_restore_owner
#print axioms held_cannot_manufacture_output
#print axioms held_cannot_add_earned_artifacts
#print axioms downstream_hold_preserves_earlier_evidence
#print axioms earlier_artifact_survives_downstream_hold
#print axioms nonempty_compatible_checks_determine_one_path

end UFormal
