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
// A lower-category student challenges a full project.
// Only feasible higher-category replacements are considered.
// The removed student must be the least-preferred among them.

bool condition4(const vector<int>& mu, int s, int p,
                const vector<int>& projectCount) {

    // Project must be full
    if (projectCount[p] < projectCapacity[p])
        return false;

    int l = projectLecturer[p];

    // Calculate category counts at lecturer l,
    // excluding incoming student s if already assigned
    vector<int> catsL(3, 0);

    for (int t = 0; t < n; ++t) {

        if (mu[t] == -1)
            continue;

        if (projectLecturer[mu[t]] == l)
            catsL[category[t]]++;
    }

    vector<int> eligibleCandidates;

    for (int t = 0; t < n; ++t) {

        if (mu[t] != p || t == s)
            continue;

        bool eligible = false;

        int oldCat = category[t];
        int newCat = category[s];

        // Condition 4.1: Inter-category displacement
        if (newCat > oldCat) {

            int lowerCount = 0;

            for (int c = oldCat + 1; c < 3; ++c)
                lowerCount += catsL[c];

            if (catsL[oldCat] > lowerCount + 1)
                eligible = true;
        }

        // Condition 4.2: Same-category merit competition
        else if (newCat == oldCat) {

            if (studentPriority[l][s] <
                studentPriority[l][t]) {

                eligible = true;
            }
        }

        if (!eligible)
            continue;

        // Test replacement feasibility
        vector<int> temp = mu;

        temp[s] = -1;
        temp[t] = -1;
        temp[s] = p;

        if (feasible(temp))
            eligibleCandidates.push_back(t);
    }

    if (eligibleCandidates.empty())
        return false;

    // Select least-preferred eligible student
    int worstStudent = eligibleCandidates[0];

    for (int t : eligibleCandidates) {

        if (studentPriority[l][t] >
            studentPriority[l][worstStudent]) {

            worstStudent = t;
        }
    }

    // Final feasibility check
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

    // Student-Proposing Deferred Acceptance
    queue<int> FreeStudents;

    vector<int> nextChoice(n, 0);

    matching.assign(n, -1);

    for (int i = 0; i < n; ++i)
        FreeStudents.push(i);

    for (int round = 0; round < 1000; ++round) {
    while (!FreeStudents.empty()) {

        int s = FreeStudents.front();
        FreeStudents.pop();

        // Student has exhausted preferences
        if (nextChoice[s] >= (int)preferences[s].size())
            continue;

        // Student proposes to next project
        int p = preferences[s][nextChoice[s]++];
        int l = projectLecturer[p];

        bool accepted = false;

        // A matched student only re-proposes to projects better than the current one
        if (matching[s] != -1 &&
            studentProjectRank[s][p] >= studentProjectRank[s][matching[s]]) {
            if (nextChoice[s] < (int)preferences[s].size()) FreeStudents.push(s);
            continue;
        }

        // Calculate current allocation statistics
        vector<int> projectCount(m, 0);
        vector<int> total(L, 0);
        vector<vector<int>> cats(L, vector<int>(3, 0));

        for (int t = 0; t < n; ++t) {
            if (matching[t] == -1)
                continue;

            int q = matching[t];
            int lecturer = projectLecturer[q];

            projectCount[q]++;
            total[lecturer]++;
            cats[lecturer][category[t]]++;
        }

        // =========================================
        // CASE A: PROJECT HAS A VACANCY
        // =========================================

        if (projectCount[p] < projectCapacity[p]) {

            int oldProject = matching[s];

            // Condition 3(a): Internal upgrade
            if (oldProject != -1 &&
                projectLecturer[oldProject] == l) {

                if (condition3a(matching, s, p, oldProject)) {

                    matching[s] = p;
                    accepted = true;
                }
            }

            // Condition 3(b): External entry
            else if (total[l] < lecturerCapacity[l]) {

                if (condition3b(
                    matching, s, p,
                    projectCount[p], total[l])) {

                    matching[s] = p;
                    accepted = true;
                }
            }

            // Condition 3(c): Lecturer is full
            else if (total[l] == lecturerCapacity[l]) {

                int worstProject = -1;
                int worstRank = -1;

                // Find least-preferred occupied project
                for (int q = 0; q < m; ++q) {

                    if (projectLecturer[q] != l ||
                        projectCount[q] == 0)
                        continue;

                    int rank = lecturerProjectRank[l][q];

                    if (rank > worstRank) {
                        worstRank = rank;
                        worstProject = q;
                    }
                }

                // Lecturer must prefer the new project
                if (worstProject != -1 &&
                    lecturerProjectRank[l][p] < worstRank) {

                    // Find a feasible outgoing student
                    for (int t = 0; t < n; ++t) {

                        if (matching[t] != worstProject)
                            continue;

                        vector<int> temp = matching;

                        temp[t] = -1;
                        temp[s] = p;

                        if (feasible(temp)) {

                            matching[t] = -1;
                            FreeStudents.push(t);

                            matching[s] = p;

                            accepted = true;
                            break;
                        }
                    }
                }
            }
        }

        // =========================================
        // CASE B: PROJECT IS FULL
        // =========================================

        else if (projectCount[p] >= projectCapacity[p]) {

            vector<int> EligibleCandidates;

            for (int t = 0; t < n; ++t) {

                if (matching[t] != p)
                    continue;

                bool eligible = false;

                // Condition 4.1:
                // Lower-category student replaces
                // higher-category student

                if (category[s] > category[t]) {

                    int lowerCategoryCount = 0;

                    for (int c = category[t] + 1; c < 3; ++c)
                        lowerCategoryCount += cats[l][c];

                    if (cats[l][category[t]] >
                        lowerCategoryCount + 1) {

                        eligible = true;
                    }
                }

                // Condition 4.2:
                // Same-category merit competition

                else if (category[s] == category[t]) {

                    if (studentPriority[l][s] <
                        studentPriority[l][t]) {

                        eligible = true;
                    }
                }

                // Check feasibility of replacement
                if (eligible) {

                    vector<int> temp = matching;

                    if (matching[s] != -1)
                        temp[s] = -1;

                    temp[t] = -1;
                    temp[s] = p;

                    if (feasible(temp))
                        EligibleCandidates.push_back(t);
                }
            }

            // Select least-preferred eligible student
            if (!EligibleCandidates.empty()) {

                int worstStudent = EligibleCandidates[0];

                for (int t : EligibleCandidates) {

                    if (studentPriority[l][t] >
                        studentPriority[l][worstStudent]) {

                        worstStudent = t;
                    }
                }

                // Displace the selected student
                matching[worstStudent] = -1;

                FreeStudents.push(worstStudent);

                // Accept the incoming student
                matching[s] = p;

                accepted = true;
            }
        }

        // =========================================
        // REJECTION: STUDENT PROPOSES AGAIN
        // =========================================

        if (!accepted) {

            if (nextChoice[s] < (int)preferences[s].size()) {
                FreeStudents.push(s);
            }
        }
    }

    // Rejections are NOT permanent here (suffix quotas change as students are
    // evicted), so re-activate any student who now has a blocking pair.
    bool changed = false;
    for (int t = 0; t < n && !changed; ++t)
        for (int q : preferences[t])
            if (isBlockingPair(matching, t, q)) {
                nextChoice[t] = studentProjectRank[t][q];
                FreeStudents.push(t);
                changed = true;
                break;
            }
    if (!changed) break;
    }

    // Save final matching
    answer = matching;

    return isStable(answer);
}



int readInt(ifstream& fin) {
    string token;

    while (fin >> token) {
        if (!token.empty() && token[0] == '#') {
            string rest;
            getline(fin, rest);
            continue;
        }

        return stoi(token);
    }

    throw runtime_error("Unexpected end of input file.");
}

void validateInput() {
    if (n < 0 || m < 0 || L < 0) {
        throw runtime_error("Invalid problem dimensions.");
    }

    if ((int)category.size() != n ||
        (int)preferences.size() != n ||
        (int)projectLecturer.size() != m ||
        (int)projectCapacity.size() != m ||
        (int)lecturerCapacity.size() != L ||
        (int)suffixCapacity.size() != L ||
        (int)lecturerProjectRank.size() != L ||
        (int)studentPriority.size() != L) {
        throw runtime_error("Input dimensions do not match.");
    }

    // Validate student categories.
    for (int s = 0; s < n; ++s) {
        if (category[s] < 0 || category[s] > 2) {
            throw runtime_error("Invalid student category.");
        }
    }

    // Validate project ownership and capacities.
    for (int p = 0; p < m; ++p) {
        if (projectLecturer[p] < 0 ||
            projectLecturer[p] >= L) {
            throw runtime_error("Invalid lecturer ID for project.");
        }

        if (projectCapacity[p] < 0) {
            throw runtime_error("Negative project capacity.");
        }
    }

    for (int l = 0; l < L; ++l) {
        if (lecturerCapacity[l] < 0) {
            throw runtime_error("Negative lecturer capacity.");
        }

        if ((int)suffixCapacity[l].size() != 3 ||
            (int)lecturerProjectRank[l].size() != m ||
            (int)studentPriority[l].size() != n) {
            throw runtime_error("Invalid lecturer data dimensions.");
        }

        // Validate suffix limits.
        for (int c = 0; c < 3; ++c) {
            if (suffixCapacity[l][c] < 0) {
                throw runtime_error("Negative suffix capacity.");
            }
        }

        if (suffixCapacity[l][0] < suffixCapacity[l][1] ||
            suffixCapacity[l][1] < suffixCapacity[l][2]) {
            throw runtime_error(
                "Suffix capacities must be non-increasing."
            );
        }

        // Validate project ranks: only own projects may be ranked.
        set<int> ranks;

        for (int p = 0; p < m; ++p) {
            int rank = lecturerProjectRank[l][p];

            if (projectLecturer[p] != l) {
                if (rank != -1) {
                    throw runtime_error(
                        "Lecturer ranks a project they do not offer."
                    );
                }
            } else {
                if (rank < 0) {
                    throw runtime_error(
                        "An offered project must have a valid rank."
                    );
                }

                if (!ranks.insert(rank).second) {
                    throw runtime_error(
                        "Duplicate project rank for a lecturer."
                    );
                }
            }
        }

        // Student priority ranks must form a permutation 0..n-1.
        vector<bool> seen(n, false);

        for (int s = 0; s < n; ++s) {
            int rank = studentPriority[l][s];

            if (rank < 0 || rank >= n || seen[rank]) {
                throw runtime_error(
                    "Invalid lecturer student-priority ranking."
                );
            }

            seen[rank] = true;
        }
    }

    // Validate student preference lists.
    for (int s = 0; s < n; ++s) {
        vector<bool> seen(m, false);

        for (int p : preferences[s]) {
            if (p < 0 || p >= m) {
                throw runtime_error(
                    "Invalid project ID in student preferences."
                );
            }

            if (seen[p]) {
                throw runtime_error(
                    "Duplicate project in a student preference list."
                );
            }

            seen[p] = true;
        }
    }
}



int main() {
    try {
        ifstream fin("data.txt");

        if (!fin.is_open()) {
            cerr << "Error: Could not open "
                 << "data.txt\n";
            cerr << "Place the file in the program's working directory.\n";
            return 1;
        }

        // 1. Read problem dimensions.
        n = readInt(fin);
        m = readInt(fin);
        L = readInt(fin);

        if (n <= 0 || m <= 0 || L <= 0) {
            throw runtime_error("Invalid problem dimensions.");
        }

        // 2. Read student categories.
        category.resize(n);

        for (int s = 0; s < n; ++s) {
            category[s] = readInt(fin);
        }

        // 3. Read lecturer offering each project.
        projectLecturer.resize(m);

        for (int p = 0; p < m; ++p) {
            projectLecturer[p] = readInt(fin);
        }

        // 4. Read project capacities.
        projectCapacity.resize(m);

        for (int p = 0; p < m; ++p) {
            projectCapacity[p] = readInt(fin);
        }

        // 5. Read lecturer capacities.
        lecturerCapacity.resize(L);

        for (int l = 0; l < L; ++l) {
            lecturerCapacity[l] = readInt(fin);
        }

        // 6. Read suffix capacities.
        suffixCapacity.assign(L, vector<int>(3));

        for (int l = 0; l < L; ++l) {
            for (int c = 0; c < 3; ++c) {
                suffixCapacity[l][c] = readInt(fin);
            }
        }

        // 7. Read lecturer project rankings.
        // -1 means the lecturer does not offer that project.
        lecturerProjectRank.assign(
            L, vector<int>(m)
        );

        for (int l = 0; l < L; ++l) {
            for (int p = 0; p < m; ++p) {
                lecturerProjectRank[l][p] = readInt(fin);
            }
        }

        // 8. Read lecturer student priorities.
        // Each row lists student IDs from most preferred
        // to least preferred.
        // Convert that order into ranks: smaller is better.
        studentPriority.assign(L, vector<int>(n));

        for (int l = 0; l < L; ++l) {
            vector<bool> seen(n, false);

            for (int rank = 0; rank < n; ++rank) {
                int s = readInt(fin);

                if (s < 0 || s >= n || seen[s]) {
                    throw runtime_error(
                        "Invalid student ID in lecturer priorities."
                    );
                }

                seen[s] = true;
                studentPriority[l][s] = rank;
            }
        }

        // 9. Read student preference lists.
        preferences.assign(n, vector<int>());

        for (int s = 0; s < n; ++s) {
            int k = readInt(fin);

            if (k < 0 || k > m) {
                throw runtime_error(
                    "Invalid preference-list length."
                );
            }

            for (int j = 0; j < k; ++j) {
                preferences[s].push_back(readInt(fin));
            }
        }

        fin.close();

        // 10. Build student-project preference ranks.
        studentProjectRank.assign(
            n, vector<int>(m, INT_MAX)
        );

        for (int s = 0; s < n; ++s) {
            for (int i = 0;
                 i < (int)preferences[s].size(); ++i) {

                int p = preferences[s][i];

                if (p < 0 || p >= m) {
                    throw runtime_error(
                        "Invalid project ID in student preferences."
                    );
                }

                if (studentProjectRank[s][p] != INT_MAX) {
                    throw runtime_error(
                        "Duplicate project in student preferences."
                    );
                }

                studentProjectRank[s][p] = i;
            }
        }

        // 11. Validate the complete input.
        validateInput();

        // 12. Initialize the matching.
        matching.assign(n, -1);

        cout << "Input loaded and validated successfully.\n";
        cout << "Students: " << n << '\n';
        cout << "Projects: " << m << '\n';
        cout << "Lecturers: " << L << "\n\n";

        // 13. Run the existing exhaustive-search algorithm.
        if (findStableMatching(0)) {
            cout << "Stable matching found:\n";

            for (int s = 0; s < n; ++s) {
                cout << "S" << s
                     << " (C" << category[s] + 1 << ") -> ";

                if (answer[s] == -1) {
                    cout << "Unassigned\n";
                } else {
                    cout << "P" << answer[s] << '\n';
                }
            }

            cout << "\nFeasibility verification: "
                 << (validMatching(answer) ? "PASSED" : "FAILED")
                 << '\n';

            cout << "Stability verification: "
                 << (isStable(answer) ? "PASSED" : "FAILED")
                 << '\n';
        } else {
            cout << "No stable matching found under "
                 << "the implemented conditions.\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
