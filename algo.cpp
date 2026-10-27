
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
bool condition4( const vector<int>& mu, int s, int p, const vector<int>& projectCount) {
    if (projectCount[p] < projectCapacity[p]) // project must be full
        return false;

    int l = projectLecturer[p];

    vector<int> feasibleCandidates;

    for (int oldStudent = 0; oldStudent < n; ++oldStudent) {
        if (mu[oldStudent] != p)
            continue;

        // Incoming student must belong to a lower category.
        if (category[s] <= category[oldStudent])
            continue;

        vector<int> temp = mu;

        // The incoming student leaves their old assignment,
        // if any. The old project can only gain free capacity.
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

    // Condition 4 holds only if this student is the
    // least-preferred eligible student.
    vector<int> temp = mu;
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

int main() {
    // Example input. Replace these values with your own data.
    n = 5;
    m = 2;
    L = 2;

    category = {0, 0, 1, 2, 2};

    preferences = {
        {0, 1},
        {0, 1},
        {0, 1},
        {0, 1},
        {0, 1}
    };

    projectLecturer = {0, 1};

    projectCapacity = {2, 3};
    lecturerCapacity = {2, 3};

    suffixCapacity = {
        {2, 2, 1},
        {3, 3, 2}
    };

    // Project preferences:
    // Smaller rank means the lecturer prefers that project more.
    lecturerProjectRank = {
        {0, 1},
        {0, 1}
    };

    // Student priorities:
    // Smaller rank means the lecturer prefers that student more.
    studentPriority.assign(L, vector<int>(n));

    for (int l = 0; l < L; ++l) {
        for (int s = 0; s < n; ++s) {
            studentPriority[l][s] = s;
        }
    }

    // Construct student preference rank lookup tables.
    studentProjectRank.assign(
        n, vector<int>(m, INT_MAX)
    );

    for (int s = 0; s < n; ++s) {
        for (int i = 0; i < (int)preferences[s].size(); ++i) {
            int p = preferences[s][i];

            if (p >= 0 && p < m)
                studentProjectRank[s][p] = i;
        }
    }

    matching.assign(n, -1);

    if (findStableMatching(0)) {
        cout << "Stable matching found:\n";

        for (int s = 0; s < n; ++s) {
            cout << "S" << s
                 << " (C" << category[s] + 1 << ") -> ";

            if (answer[s] == -1)
                cout << "unassigned\n";
            else
                cout << "P" << answer[s] << '\n';
        }

        cout << "\nStability verification: "
             << (isStable(answer) ? "PASSED" : "FAILED")
             << '\n';
    } else {
        cout << "No stable matching found under the "
             << "implemented conditions.\n";
    }

    return 0;
}
