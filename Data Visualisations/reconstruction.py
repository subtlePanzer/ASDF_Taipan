import csv

# ============================================================
# 1. SCORING RULES
# Edit these values if the rules change.
# ============================================================

YELLOW_VALUE = 10
COLOURED_VALUE = 5
AUTONOMOUS_BONUS = 12
AUTONOMOUS_END = 15

MIDZONE_ROBOT_VALUE = 8


MATCH_TEAM_TO_ALLIANCE = {
    "T1_Q7": {
        "41103X": "BLUE",
        "41103C": "BLUE",
        "12906A": "RED",
        "23140A": "RED",
    },

    "T1_Q13": {
        "41103X": "RED",
        "49898G": "RED",
        "12223F": "BLUE",
        "76981A": "BLUE"
    },

    "T1_Q16": {
        "12906B": "BLUE",
        "49898G": "BLUE",
        "41103X": "RED",
        "81321G": "RED"
    },

    "T1_Q17": {
            "41103X": "BLUE",
            "76981A": "BLUE",
            "41103C": "RED",
            "12906A": "RED"
    },

    "T1_Q5": {
            "12906B": "RED",
            "41103X": "RED",
            "52111A": "BLUE",
            "12223F": "BLUE"
    },

    "T1_Q20": {
            "76981B": "RED",
            "49898G": "RED",
            "41103X": "BLUE",
            "41103C": "BLUE"
    },

    "T1_SF1": {
                "41103X": "RED",
                "41103C": "RED",
                "12906A": "BLUE",
                "81321C": "BLUE"
    },

    "T1_F1": {
                "41103X": "RED",
                "41103C": "RED",
                "52111A": "BLUE",
                "81321D": "BLUE"
    },

    "T2_Q2": {
                "41103X": "RED",
                "50911B": "RED",
                "69153A": "BLUE",
                "81321B": "BLUE"
    },

    "T2_Q10": {
                "81321D": "RED",
                "68076H": "RED",
                "41103X": "BLUE",
                "52111A": "BLUE"
    },

    "T2_Q12": {
                "75477J": "RED",
                "75477M": "RED",
                "41103X": "BLUE",
                "14671A": "BLUE"
    },

    "T2_Q20": {
                "41103X": "RED",
                "12223X": "RED",
                "76981B": "BLUE",
                "81321C": "BLUE"
    }
}

# ============================================================
# 2. INITIAL FIELD
# ============================================================

def initial_field():

    return {
        "toggles": {
            "Q1": "NEUTRAL",
            "Q2": "NEUTRAL",
            "Q3": "NEUTRAL",
            "Q4": "NEUTRAL",
        },

        "goals": {
            "GOAL_LOW_Q1": [],
            "GOAL_LOW_Q2": [],
            "GOAL_LOW_Q3": [],
            "GOAL_LOW_Q4": [],

            "GOAL_HIGH_Q1": ["YELLOW"],
            "GOAL_HIGH_Q2": ["YELLOW"],
            "GOAL_HIGH_Q3": ["YELLOW"],
            "GOAL_HIGH_Q4": ["YELLOW"],

            "CENTRE_GOAL": ["YELLOW"],
        },

        "violations": [],
    }


# ============================================================
# 3. APPLY PHYSICAL EVENTS
# ============================================================

def apply_event(field, event):

    event_type = event["Event_Type"].upper()
    outcome = event["Outcome"].upper()
    score_effect = event["Score_Effect"].upper()

    actor = event["Actor_ID"]
    alliance = TEAM_TO_ALLIANCE.get(actor)

    target = event.get("Target_ID", "")

    # --------------------------------------------------------
    # TOGGLE FLIPS
    # --------------------------------------------------------

    if event_type == "TOGGLE_FLIP":

        quadrant = target.replace("TOGGLE_", "")

        after = event.get("Toggle_State_After", "").upper()

        if after in ["RED", "BLUE", "NEUTRAL"]:
            field["toggles"][quadrant] = after

        elif outcome == "SUCCESS" and alliance:
            field["toggles"][quadrant] = alliance

    # --------------------------------------------------------
    # PIN PLACEMENTS
    # --------------------------------------------------------

    elif event_type in ["PLACE_YELLOW", "PLACE_COLOURED"]:

        # A PARTIAL placement should only count if a pin
        # actually remained in the goal.
        pin_was_placed = (
            outcome == "SUCCESS"
            or (
                outcome == "PARTIAL"
                and event.get("Pin_Placed", "").upper() == "TRUE"
            )
        )

        if not pin_was_placed:
            return

        if target not in field["goals"]:
            return

        if score_effect == "0":
            ## print("net zero change worked")
            return

        if event_type == "PLACE_YELLOW" and score_effect == "15":
            field["goals"][target].pop()
            field["goals"][target].append("YELLOW")
            field["goals"][target].append("YELLOW")

        elif event_type == "PLACE_YELLOW" and score_effect == "5":
                    field["goals"][target].pop()
                    field["goals"][target].append("YELLOW")

        elif event_type == "PLACE_COLOURED" and score_effect == "15":
                    field["goals"][target].pop()
                    field["goals"][target].append("YELLOW")
                    field["goals"][target].append(alliance)

        elif event_type == "PLACE_YELLOW":
            field["goals"][target].append("YELLOW")

        elif event_type == "PLACE_COLOURED" and alliance:
            field["goals"][target].append(alliance)

    # --------------------------------------------------------
    # DESCORING
    # --------------------------------------------------------

    elif event_type.startswith("DESCORE"):

        if outcome != "SUCCESS":
            return

        if target not in field["goals"]:
            return

        pin_colour = event.get("Pin_colour", "").upper()

        if not pin_colour:
            return

        goal = field["goals"][target]

        if pin_colour in goal:
            goal.remove(pin_colour)
            ## print("descore works")

            field["violations"].append({
                "team": actor,
                "event_id": event["Event_ID"],
                "type": "MAJOR_VIOLATION",
            })


# ============================================================
# 4. CALCULATE ORDINARY FIELD SCORE
# ============================================================

def calculate_score(field):

    scores = {
        "RED": 0,
        "BLUE": 0,
    }

    for goal, pins in field["goals"].items():

        for pin in pins:

            # Coloured pins score for their owning alliance.
            if pin in ["RED", "BLUE"]:
                scores[pin] += COLOURED_VALUE

            # Yellow pins in quadrant goals score for the
            # alliance controlling the corresponding toggle.
            elif pin == "YELLOW":

                if goal.startswith("GOAL_LOW_") or \
                   goal.startswith("GOAL_HIGH_"):

                    quadrant = goal[-2:]
                    controller = field["toggles"][quadrant]

                    if controller in ["RED", "BLUE"]:
                        scores[controller] += YELLOW_VALUE

    return scores


# ============================================================
# 5. AUTONOMOUS BONUS
# ============================================================


def calculate_autonomous_bonus(field):

    scores = calculate_score(field)

    # If any points have been scored, compare points.
    if scores["RED"] > 0 or scores["BLUE"] > 0:

        if scores["RED"] > scores["BLUE"]:
            return {"RED": 12, "BLUE": 0}

        elif scores["BLUE"] > scores["RED"]:
            return {"RED": 0, "BLUE": 12}

        # Equal points: 6 points to each.
        return {"RED": 6, "BLUE": 6}

    # Only when neither alliance has scored any points
    # do we use toggle control as the tiebreaker.
    red_toggles = list(field["toggles"].values()).count("RED")
    blue_toggles = list(field["toggles"].values()).count("BLUE")

    if red_toggles > blue_toggles:
        return {"RED": 12, "BLUE": 0}

    elif blue_toggles > red_toggles:
        return {"RED": 0, "BLUE": 12}

    # Equal toggle control: 6 points to each.
    return {"RED": 6, "BLUE": 6}


# ============================================================
# 6. IDENTIFY ROBOTS IN THE MIDZONE AT MATCH END
# ============================================================

def get_midzone_robots(events):

    robots = set()

    for event in events:

        actor = event["Actor_ID"]

        # Alternative: use a successful ENDGAME event
        # explicitly targeting the midzone.
        event_type = event["Event_Type"].upper()
        target_id = event.get("Target_ID", "").upper()

        if (
            event_type == "ENDGAME"
            and event["Outcome"].upper() == "SUCCESS"
            and target_id == "MIDZONE"
        ):
            robots.add(actor)

    return robots


# ============================================================
# 7. APPLY ENDGAME AND CENTRE-GOAL SCORING
# ============================================================

def calculate_endgame_score(field, events):

    scores = {
        "RED": 0,
        "BLUE": 0,
    }

    # Endgame scoring is only evaluated at match end.
    midzone_robots = get_midzone_robots(events)

    robot_counts = {
        "RED": 0,
        "BLUE": 0,
    }

    for robot in midzone_robots:

        alliance = TEAM_TO_ALLIANCE.get(robot)

        if alliance:
            robot_counts[alliance] += 1
            scores[alliance] += MIDZONE_ROBOT_VALUE

    # Centre-goal yellow pins are awarded to the alliance
    # with more robots in the midzone.
    # If robot counts are equal, they score for neither.
    centre_pins = field["goals"]["CENTRE_GOAL"].count("YELLOW")

    if robot_counts["RED"] > robot_counts["BLUE"]:
        scores["RED"] += centre_pins * YELLOW_VALUE

    elif robot_counts["BLUE"] > robot_counts["RED"]:
        scores["BLUE"] += centre_pins * YELLOW_VALUE

    return scores, robot_counts, midzone_robots


# ============================================================
# 8. MAIN RECONSTRUCTION FUNCTION
# ============================================================

def reconstruct(events, time):

    field = initial_field()

    # Reconstruct the physical field at the requested time.
    for event in events:

        if float(event["Event_end_sec"]) <= time:
            apply_event(field, event)

    # Calculate ordinary scoring.
    scores = calculate_score(field)

    # Autonomous bonus: evaluate the field at 15 seconds.
    bonus = {"RED": 0, "BLUE": 0}

    if time >= AUTONOMOUS_END:

        auto_field = initial_field()

        for event in events:

            if float(event["Event_end_sec"]) <= AUTONOMOUS_END:
                apply_event(auto_field, event)

        bonus = calculate_autonomous_bonus(auto_field)

    # Endgame and centre-goal scoring are only evaluated
    # at the end of the match (120 seconds).
    endgame = {"RED": 0, "BLUE": 0}
    robot_counts = {"RED": 0, "BLUE": 0}
    midzone_robots = set()

    if time >= 120:

        endgame, robot_counts, midzone_robots = (
            calculate_endgame_score(field, events)
        )

    final_scores = {
        alliance: (
            scores[alliance]
            + bonus[alliance]
            + endgame[alliance]
        )
        for alliance in ["RED", "BLUE"]
    }

    return {
        "field": field,
        "base_scores": scores,
        "autonomous_bonus": bonus,
        "endgame_score": endgame,
        "midzone_robot_counts": robot_counts,
        "midzone_robots": sorted(midzone_robots),
        "final_scores": final_scores,
        "violations": field["violations"],
    }


# ============================================================
# 9. IMPORT CSV AND SELECT MATCH
# ============================================================

with open("events.csv", newline="", encoding="utf-8-sig") as file:
    events = list(csv.DictReader(file))

# Identify available matches.
match_ids = sorted(set(event["Match_ID"] for event in events))

print("\nAvailable matches:")
for match_id in match_ids:
    print(match_id)

# Ask which match to analyse.
selected_match = input("\nEnter Match ID: ").strip()

# Filter events to the selected match only.
events = [
    event for event in events
    if event["Match_ID"] == selected_match
]

if not events:
    raise ValueError("Invalid Match ID. Please check the available matches.")

# Load the correct alliance mapping for the selected match.
if selected_match not in MATCH_TEAM_TO_ALLIANCE:
    raise ValueError("No alliance mapping exists for this Match ID.")

TEAM_TO_ALLIANCE = MATCH_TEAM_TO_ALLIANCE[selected_match]

# Sort the selected match's events chronologically.
events.sort(
    key=lambda event: float(event["Event_end_sec"])
)

t = 120 - float(input("How many seconds left? "))
result = reconstruct(events, t)

print("\n--- MATCH SCORE ---")
print("RED:", result["final_scores"]["RED"])
print("BLUE:", result["final_scores"]["BLUE"])

print("\n--- SCORE BREAKDOWN ---")
print("Ordinary:", result["base_scores"])
print("Autonomous:", result["autonomous_bonus"])
print("Endgame:", result["endgame_score"])

print("\n--- MIDZONE ---")
print("Robot counts:", result["midzone_robot_counts"])
print("Robots:", result["midzone_robots"])

print("\n--- FIELD ---")
print("Toggles:", result["field"]["toggles"])
print("Violations:", result["violations"])

print("\nPINS IN EACH GOAL")

for goal, pins in result["field"]["goals"].items():
    print(f"{goal}: {pins}")