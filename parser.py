"""Helpers: parse the data file header and the C++ program's output."""
import re


def parse_data(text: str) -> dict:
    """Pull out what the UI needs from data.txt (n, m, L, categories, project->lecturer, capacities)."""
    lines = [l.strip() for l in text.splitlines() if l.strip() and not l.lstrip().startswith("#")]
    n, m, L = map(int, lines[0].split())
    categories = list(map(int, lines[1].split()))
    project_lecturer = list(map(int, lines[2].split()))
    project_capacity = list(map(int, lines[3].split()))
    lecturer_capacity = list(map(int, lines[4].split()))
    if len(categories) != n or len(project_lecturer) != m:
        raise ValueError("Header sizes do not match n / m.")
    return {
        "n": n, "m": m, "L": L,
        "categories": categories,
        "project_lecturer": project_lecturer,
        "project_capacity": project_capacity,
        "lecturer_capacity": lecturer_capacity,
    }


_ASSIGN = re.compile(r"^S(\d+)\s+\(C(\d)\)\s+->\s+(Unassigned|P(\d+))\s*$")


def parse_output(stdout: str) -> dict:
    assignments = []
    for line in stdout.splitlines():
        m = _ASSIGN.match(line.strip())
        if m:
            assignments.append({
                "student": int(m.group(1)),
                "category": int(m.group(2)),
                "project": None if m.group(3) == "Unassigned" else int(m.group(4)),
            })
    return {
        "found": "Stable matching found" in stdout,
        "feasible": "Feasibility verification: PASSED" in stdout,
        "stable": "Stability verification: PASSED" in stdout,
        "assignments": assignments,
    }
