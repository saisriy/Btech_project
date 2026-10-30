
#include <bits/stdc++.h>
using namespace std;

// Categories: 0 = C1 (highest), 1 = C2, 2 = C3 (lowest).
// A smaller preference rank means a more-preferred option.

int n, m, L;
// n = no. of students
// m = no. of projects
// L = no. of lecturers

vector<int> category;           // category of each student
vector<vector<int>> preferences;// preference list of each student
vector<int> projectLecturer;    // lecturer of each project
vector<int> projectCapacity;    // capacity of each project
vector<int> lecturerCapacity;   // capacity of each lecturer

// Lecturer's project preference ranks.
vector<vector<int>> lecturerProjectRank;    
// --> here project id is the index and the value is the rank of that project. if any value is -1, the lecturer does not offer that project 

// Lecturer's student-priority ranks.
vector<vector<int>> studentPriority;


// Suffix capacities:
// [0] = C1 + C2 + C3
// [1] = C2 + C3
// [2] = C3
vector<vector<int>> suffixCapacity;

vector<int> matching;        // current assignment
vector<int> answer;          // final answer

vector<vector<int>> studentsAtProject;
vector<int> lecturerProjectCount;
vector<int> lecturerCount;

vector<vector<int>> studentProjectRank;  // ranks all the projects of a student from the preference list.
// index is project and value is rank. smaller rank - higher preference. 
// if value = INT_MAX - the project is unacceptable.


// Checks validity
bool validMatching(const vector<int>& mu) {
    vector<int> projectCount(m, 0);                 // keeps track of no. of students assigned to each project
    vector<int> total(L, 0);                        // track of total number of students assigned to lecturer l
    vector<vector<int>> cats(L, vector<int>(3, 0)); // track of no. of cat[i] students assigned to each lecturer

    for (int s = 0; s < n; ++s) {
        int p = mu[s];

        if (p == -1) continue;

        if (p < 0 || p >= m) return false;

        // A student may only receive an acceptable project.
        if (studentProjectRank[s][p] == INT_MAX)
            return false;

        int l = projectLecturer[p];

        ++projectCount[p];
        ++total[l];
        ++cats[l][category[s]];
    }

    for (int p = 0; p < m; ++p) {
        if (projectCount[p] > projectCapacity[p])
            return false;
    }

    for (int l = 0; l < L; ++l) {
        if (total[l] > lecturerCapacity[l])
            return false;

        if (cats[l][0] + cats[l][1] + cats[l][2]
            > suffixCapacity[l][0])
            return false;

        if (cats[l][1] + cats[l][2]
            > suffixCapacity[l][1])
            return false;

        if (cats[l][2] > suffixCapacity[l][2])
            return false;
    }

    return true;
}

// Simulate a matching change and verify every capacity constraint.
bool feasible(const vector<int>& mu) {
    return validMatching(mu);
}

// Condition 3(a):
// Student moves between two projects offered by the same lecturer.
bool condition3a( const vector<int>& mu, int s, int p, int oldProject) {
    int l = projectLecturer[p];

    if (projectLecturer[oldProject] != l)
        return false;

    if (lecturerProjectRank[l][p]
        >= lecturerProjectRank[l][oldProject])
        return false;

    vector<int> temp = mu;
    temp[s] = p;

    return feasible(temp);
}

// Condition 3(b):
// Project has space and the new lecturer has spare capacity.
bool condition3b(const vector<int>& mu, int s, int p, int projectCount, int totalAtLecturer) {
    int l = projectLecturer[p];

    if (projectCount >= projectCapacity[p])
        return false;

    if (totalAtLecturer >= lecturerCapacity[l])
        return false;

    vector<int> temp = mu;
    temp[s] = p;

    return feasible(temp);
}

// Condition 3(c):
// Lecturer is full. The lecturer must prefer p over the
// least-preferred currently occupied project.
// Try removing a student from that project and adding s.
//
// Assumption: the outgoing student is selected from the
// least-preferred occupied project, as described in the prompt.
bool condition3c( const vector<int>& mu, int s, int p, const vector<int>& projectCount, int totalAtLecturer) {
    int l = projectLecturer[p];

    if (projectCount[p] >= projectCapacity[p])
        return false;

    if (totalAtLecturer < lecturerCapacity[l])
        return false;

    int worstProject = -1;
    int worstRank = -1;

    for (int q = 0; q < m; ++q) {
        if (projectLecturer[q] != l)
            continue;

        if (projectCount[q] == 0)
            continue;

        int rank = lecturerProjectRank[l][q];

        if (rank > worstRank) {
            worstRank = rank;
            worstProject = q;
        }
    }

    if (worstProject == -1)
        return false;

    // The lecturer must prefer p over the worst occupied project.
    if (lecturerProjectRank[l][p] >= worstRank)
        return false;

    for (int outgoing = 0; outgoing < n; ++outgoing) {
        if (mu[outgoing] != worstProject)
            continue;

        vector<int> temp = mu;

        // Remove the outgoing student.
        temp[outgoing] = -1;

        // Assign the incoming student to p.
        temp[s] = p;

        if (feasible(temp))
            return true;
    }

    return false;
}

// Condition 4:
// Project is full.
// 1. A lower-category student can replace a higher-category student only when
//    there is an inter-category violation at the higher category:
//    |mu(l) ∩ y| > |mu(l) ∩ (Suff(y) \ {y})| + 1.
// 2. Within the same category, students preferred by the lecturer (smaller studentPriority)
//    can displace less preferred students.
bool condition4(const vector<int>& mu, int s, int p, const vector<int>& projectCount) {
    if (projectCount[p] < projectCapacity[p]) // project must be full
        return false;

    int l = projectLecturer[p];

    // Count students of each category currently assigned to lecturer l
    vector<int> catsL(3, 0);
    for (int t = 0; t < n; ++t) {
        if (mu[t] != -1 && projectLecturer[mu[t]] == l) {
            ++catsL[category[t]];
        }
    }

    vector<int> feasibleCandidates;

    for (int oldStudent = 0; oldStudent < n; ++oldStudent) {
        if (mu[oldStudent] != p)
            continue;

        int oldCat = category[oldStudent];
        int sCat = category[s];
        bool eligible = false;

        // Case 1: Lower-category student challenges higher-category student (sCat > oldCat)
        // Only allowed if there is an inter-category violation at oldCat:
        // |mu(l) ∩ oldCat| > |mu(l) ∩ (Suff(oldCat) \ {oldCat})| + 1
        if (sCat > oldCat) {
            int lowerCount = 0;
            for (int z = oldCat + 1; z < 3; ++z) {
                lowerCount += catsL[z];
            }
            if (catsL[oldCat] > lowerCount + 1) {
                eligible = true;
            }
        }
        // Case 2: Within the same category, incoming student must be strictly
        // preferred by the lecturer over oldStudent
        else if (sCat == oldCat) {
            if (studentPriority[l][s] < studentPriority[l][oldStudent]) {
                eligible = true;
            }
        }

        if (!eligible)
            continue;

        vector<int> temp = mu;

        // The incoming student leaves their old assignment, if any.
        temp[s] = -1;

        // Remove the candidate from p.
        temp[oldStudent] = -1;

        // Place the incoming student at p.
        temp[s] = p;

        if (feasible(temp))
            feasibleCandidates.push_back(oldStudent);
    }

    if (feasibleCandidates.empty())
        return false;

    // Among feasible candidates, choose the least-preferred
    // student according to this lecturer's priority ranking.
    int worstStudent = feasibleCandidates[0];

    for (int candidate : feasibleCandidates) {
        if (studentPriority[l][candidate]
            > studentPriority[l][worstStudent]) {
            worstStudent = candidate;
        }
    }

    // Verify final replacement feasibility
    vector<int> temp = mu;
    temp[s] = -1;
    temp[worstStudent] = -1;
    temp[s] = p;

    return feasible(temp);
}

// Return true if (s,p) blocks the matching under the
// supplied Conditions 1-4.
bool isBlockingPair(const vector<int>& mu, int s, int p) {
    // Condition 1: project must be acceptable.
    if (studentProjectRank[s][p] == INT_MAX)
        return false;

    // Condition 2: student is unassigned or prefers p
    // to their current assignment.
    int oldProject = mu[s];

    if (oldProject == p)
        return false;

    if (oldProject != -1 &&
        studentProjectRank[s][p]
        >= studentProjectRank[s][oldProject]) {
        return false;
    }

    int l = projectLecturer[p];

    vector<int> projectCount(m, 0);
    vector<int> total(L, 0);

    for (int t = 0; t < n; ++t) {
        if (mu[t] == -1)
            continue;

        int q = mu[t];

        ++projectCount[q];
        ++total[projectLecturer[q]];
    }

    // Condition 3: project has a vacancy.
    if (projectCount[p] < projectCapacity[p]) {
        // Condition 3(a): currently assigned under the same lecturer.
        if (oldProject != -1 &&
            projectLecturer[oldProject] == l) {
            if (condition3a(mu, s, p, oldProject))
                return true;
        } else {
            // Condition 3(b): lecturer has spare capacity.
            if (condition3b(
                mu, s, p, projectCount[p], total[l]
            )) {
                return true;
            }

            // Condition 3(c): lecturer is full.
            if (condition3c(mu, s, p, projectCount, total[l]))
                return true;
        }
    }

    // Condition 4: project is full.
    if (projectCount[p] >= projectCapacity[p]) {
        if (condition4(mu, s, p, projectCount))
            return true;
    }

    return false;
}

bool isStable(const vector<int>& mu) {
    if (!validMatching(mu))
        return false;

    for (int s = 0; s < n; ++s) {
        for (int p : preferences[s]) {
            if (isBlockingPair(mu, s, p))
                return false;
        }
    }

    return true;
}

// Enumerate every possible assignment.
// -1 means unassigned.
// Stop at the first valid stable matching.
bool findStableMatching(int s) {
    if (s == n) {
        if (isStable(matching)) {
            answer = matching;
            return true;
        }

        return false;
    }

    // Try each acceptable project in preference order.
    for (int p : preferences[s]) {
        matching[s] = p;

        if (validMatching(matching) &&
            findStableMatching(s + 1)) {
            return true;
        }
    }

    // Also allow the student to remain unassigned.
    matching[s] = -1;

    if (validMatching(matching) &&
        findStableMatching(s + 1)) {
        return true;
    }

    return false;
}

void runTest(
    const string& title,
    int in_n, int in_m, int in_L,
    vector<int> in_category,
    vector<vector<int>> in_preferences,
    vector<int> in_projectLecturer,
    vector<int> in_projectCapacity,
    vector<int> in_lecturerCapacity,
    vector<vector<int>> in_suffixCapacity,
    vector<vector<int>> in_lecturerProjectRank,
    vector<vector<int>> in_studentPriority
) {
    cout << "========================================================\n";
    cout << "Test Case: " << title << "\n";
    cout << "========================================================\n";

    n = in_n;
    m = in_m;
    L = in_L;
    category = in_category;
    preferences = in_preferences;
    projectLecturer = in_projectLecturer;
    projectCapacity = in_projectCapacity;
    lecturerCapacity = in_lecturerCapacity;
    suffixCapacity = in_suffixCapacity;
    lecturerProjectRank = in_lecturerProjectRank;
    studentPriority = in_studentPriority;

    studentProjectRank.assign(n, vector<int>(m, INT_MAX));
    for (int s = 0; s < n; ++s) {
        for (int i = 0; i < (int)preferences[s].size(); ++i) {
            int p = preferences[s][i];
            if (p >= 0 && p < m)
                studentProjectRank[s][p] = i;
        }
    }

    matching.assign(n, -1);
    answer.assign(n, -1);

    if (findStableMatching(0)) {
        cout << "Stable matching found:\n";
        for (int s = 0; s < n; ++s) {
            cout << "  S" << s << " (C" << category[s] + 1 << ") -> ";
            if (answer[s] == -1)
                cout << "unassigned\n";
            else
                cout << "P" << answer[s] << "\n";
        }
        bool stable = isStable(answer);
        cout << "Stability verification: " << (stable ? "PASSED" : "FAILED") << "\n\n";
    } else {
        cout << "No stable matching found under the implemented conditions.\n\n";
    }
}

int main() {
    // ----------------------------------------------------
    // Instance 1: Base Case (No Inter-Category Violation)
    // S0 in C1, S1 in C3. P0 cap = 1.
    // |C1| = 1 <= 0 + 1, so S1 cannot displace S0.
    // ----------------------------------------------------
    runTest(
        "1. Base Case: S0(C1) vs S1(C3), No Violation (|C1| <= |C2+C3|+1)",
        2, 1, 1,
        {0, 2},                     // categories: S0=C1, S1=C3
        {{0}, {0}},                 // both want P0
        {0},                        // P0 offered by L0
        {1},                        // P0 cap = 1
        {1},                        // L0 cap = 1
        {{1, 1, 1}},                // suffix caps
        {{0}},                      // L0 project rank
        {{0, 1}}                    // L0 student priority: S0 > S1
    );

    // ----------------------------------------------------
    // Instance 2: Inter-Category Violation Triggered!
    // P0 cap = 2. S0(C1), S1(C1), S2(C3).
    // If S0 and S1 take P0, |C1| = 2 > 0 + 1 (Violation!).
    // S2(C3) displaces least preferred C1 student (S1).
    // ----------------------------------------------------
    runTest(
        "2. Inter-Category Violation: |C1| > |C2+C3|+1 allows S2(C3) to displace S1(C1)",
        3, 1, 1,
        {0, 0, 2},                  // S0=C1, S1=C1, S2=C3
        {{0}, {0}, {0}},            // all want P0
        {0},                        // P0 offered by L0
        {2},                        // P0 cap = 2
        {2},                        // L0 cap = 2
        {{2, 2, 1}},                // suffix caps: L0 can take up to 1 C3 student
        {{0}},                      // L0 project rank
        {{0, 1, 2}}                 // L0 student priority: S0 > S1 > S2
    );

    // ----------------------------------------------------
    // Instance 3: Same-Category Competition (Lecturer Priority)
    // S0 and S1 both in C2. P0 cap = 1.
    // Lecturer prefers S1 over S0 -> S1 wins P0.
    // ----------------------------------------------------
    runTest(
        "3. Same Category: S0(C2) vs S1(C2), Lecturer strictly prefers S1",
        2, 1, 1,
        {1, 1},                     // S0=C2, S1=C2
        {{0}, {0}},                 // both want P0
        {0},                        // P0 offered by L0
        {1},                        // P0 cap = 1
        {1},                        // L0 cap = 1
        {{1, 1, 1}},                // suffix caps
        {{0}},                      // L0 project rank
        {{1, 0}}                    // L0 student priority: S1(rank 0) > S0(rank 1)
    );

    // ----------------------------------------------------
    // Instance 4: Suffix Capacity Limits (Hard Quota Binding)
    // P0 cap = 2, L0 cap = 2.
    // But L0 suffixCapacity[C3] = 0 (cannot accept any C3 student).
    // S1(C3) cannot be admitted despite project vacancy.
    // ----------------------------------------------------
    runTest(
        "4. Suffix Capacity Binding: C3 suffix cap is 0, so S1(C3) rejected despite vacancy",
        2, 1, 1,
        {0, 2},                     // S0=C1, S1=C3
        {{0}, {0}},                 // both want P0
        {0},                        // P0 offered by L0
        {2},                        // P0 cap = 2
        {2},                        // L0 cap = 2
        {{2, 2, 0}},                // suffix caps: C3 cap is 0!
        {{0}},                      // L0 project rank
        {{0, 1}}                    // L0 student priority
    );

    // ----------------------------------------------------
    // Instance 5: Multi-Project Allocation with Different Preferences
    // 2 projects P0, P1 under L0. S0(C1) wants P1, S1(C1) wants P0.
    // Both accommodated at their 1st preferences.
    // ----------------------------------------------------
    runTest(
        "5. Multi-Project Allocation: S0 wants P1, S1 wants P0",
        2, 2, 1,
        {0, 0},                     // S0=C1, S1=C1
        {{1}, {0}},                 // S0 wants P1, S1 wants P0
        {0, 0},                     // P0 and P1 both offered by L0
        {1, 1},                     // P0 cap = 1, P1 cap = 1
        {2},                        // L0 cap = 2
        {{2, 2, 2}},                // suffix caps
        {{0, 1}},                   // L0 project rank: P0 > P1
        {{0, 1}}                    // L0 student priority
    );

    // ----------------------------------------------------
    // Instance 6: Lecturer Overall Capacity Bottleneck (Condition 3c)
    // P0 cap = 1, P1 cap = 1. But Lecturer capacity = 1!
    // Lecturer prefers P0 over P1.
    // S0 wants P1, S1 wants P0.
    // S1 gets P0; S0 cannot get P1 because L0 is full.
    // ----------------------------------------------------
    runTest(
        "6. Lecturer Capacity Bottleneck: L0 capacity is 1, prefers P0 over P1",
        2, 2, 1,
        {0, 0},                     // S0=C1, S1=C1
        {{1}, {0}},                 // S0 wants P1, S1 wants P0
        {0, 0},                     // P0 and P1 offered by L0
        {1, 1},                     // P0 cap = 1, P1 cap = 1
        {1},                        // L0 cap = 1! (Bottleneck)
        {{1, 1, 1}},                // suffix caps
        {{0, 1}},                   // L0 project rank: P0 > P1
        {{0, 1}}                    // L0 student priority
    );

    // ----------------------------------------------------
    // Instance 7: 5 Students, 2 Lecturers, 4 Projects
    // L0 offers P0(cap 1) and P1(cap 1), L0 cap = 2
    // L1 offers P2(cap 1) and P3(cap 2), L1 cap = 2
    // S0, S1 in C1; S2 in C2; S3, S4 in C3
    // Suffix caps: L0={2, 2, 1}, L1={2, 2, 1}
    // ----------------------------------------------------
    runTest(
        "7. 5 Students, 2 Lecturers, 4 Projects (Cross-lecturer preference)",
        5, 4, 2,
        {0, 0, 1, 2, 2},            // S0=C1, S1=C1, S2=C2, S3=C3, S4=C3
        {
            {0, 2},                 // S0: prefers P0, then P2
            {0, 1, 3},              // S1: prefers P0, then P1, then P3
            {2, 1},                 // S2: prefers P2, then P1
            {3, 0},                 // S3: prefers P3, then P0
            {3, 1}                  // S4: prefers P3, then P1
        },
        {0, 0, 1, 1},               // projectLecturer: P0,P1 under L0; P2,P3 under L1
        {1, 1, 1, 2},               // projectCapacity
        {2, 2},                     // lecturerCapacity: L0=2, L1=2
        {
            {2, 2, 1},              // L0 suffixCapacity (max 1 C3)
            {2, 2, 1}               // L1 suffixCapacity (max 1 C3)
        },
        {
            {0, 1, -1, -1},         // L0 project preference: P0 > P1
            {-1, -1, 0, 1}          // L1 project preference: P2 > P3
        },
        {
            {0, 1, 2, 3, 4},        // L0 student priority: S0 > S1 > S2 > S3 > S4
            {0, 1, 2, 3, 4}         // L1 student priority: S0 > S1 > S2 > S3 > S4
        }
    );

    // ----------------------------------------------------
    // Instance 8: 4 Students, 2 Lecturers, 3 Projects
    // L0 offers P0(cap 1), P1(cap 1), L0 cap = 2
    // L1 offers P2(cap 2), L1 cap = 2
    // S0 in C1, S1 in C1, S2 in C2, S3 in C3
    // ----------------------------------------------------
    runTest(
        "8. 4 Students, 2 Lecturers, 3 Projects (Balanced multi-capacity)",
        4, 3, 2,
        {0, 0, 1, 2},               // S0=C1, S1=C1, S2=C2, S3=C3
        {
            {0, 2},                 // S0: P0, P2
            {1, 2},                 // S1: P1, P2
            {0, 1},                 // S2: P0, P1
            {2, 0}                  // S3: P2, P0
        },
        {0, 0, 1},                  // P0, P1 under L0; P2 under L1
        {1, 1, 2},                  // projectCapacity: P0=1, P1=1, P2=2
        {2, 2},                     // lecturerCapacity: L0=2, L1=2
        {
            {2, 2, 1},              // L0 suffixCapacity
            {2, 2, 1}               // L1 suffixCapacity
        },
        {
            {0, 1, -1},             // L0 project preference: P0 > P1
            {-1, -1, 0}             // L1 project preference: P2
        },
        {
            {0, 1, 2, 3},           // L0 student priority
            {0, 1, 2, 3}            // L1 student priority
        }
    );

    return 0;
}