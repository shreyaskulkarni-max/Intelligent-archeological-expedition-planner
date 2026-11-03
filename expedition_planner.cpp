
#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

struct Chamber {
    int id;
    string name;
    int cost;                  
    int value;                 
    vector<string> grants;     
};

struct Passage {
    int from, to;
    vector<string> requires_;  
    bool active;               
};


struct Event {
    int trigger;
    int passage;
    bool activate;
    string message;
};

class Site {
public:
    vector<Chamber> chambers;
    vector<Passage> passages;
    vector<vector<int>> adj;   

    int addChamber(const string& name, int cost, int value,
                   const vector<string>& grants = {}) {
        int id = (int)chambers.size();
        chambers.push_back({id, name, cost, value, grants});
        adj.emplace_back();
        return id;
    }

    int addPassage(int from, int to, const vector<string>& req = {},
                   bool active = true) {
        int idx = (int)passages.size();
        passages.push_back({from, to, req, active});
        adj[from].push_back(idx);
        return idx;
    }

    void setActive(int passageIdx, bool a) { passages[passageIdx].active = a; }
};


struct Candidate {
    double ratio;
    int value;
    int chamber;
    
    bool operator<(const Candidate& o) const {
        if (ratio != o.ratio) return ratio < o.ratio;
        if (value != o.value) return value < o.value;
        return chamber > o.chamber;            
    }
};

struct PlanResult {
    vector<int> order;     
    int totalValue = 0;
    int totalCost = 0;
};

static bool hasAll(const vector<string>& need, const unordered_set<string>& have) {
    for (const auto& a : need)
        if (!have.count(a)) return false;
    return true;
}

static string join(const vector<string>& v) {
    string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + v[i];
    return s;
}

class Planner {
public:
   
    static PlanResult plan(Site site, int start, int budget,
                           const vector<Event>& events = {},
                           bool verbose = true) {
        const int n = (int)site.chambers.size();
        vector<bool> visited(n, false);
        unordered_set<string> artifacts;
        PlanResult res;
        int remaining = budget;

        auto visit = [&](int c, bool isStart) {
            visited[c] = true;
            for (const auto& a : site.chambers[c].grants) artifacts.insert(a);
            if (!isStart) {
                res.order.push_back(c);
                res.totalValue += site.chambers[c].value;
                res.totalCost += site.chambers[c].cost;
                remaining -= site.chambers[c].cost;
            }
        };

        visit(start, true);
        if (verbose)
            cout << "  Start at \"" << site.chambers[start].name
                 << "\"  (budget = " << budget << ")\n";

        int step = 0;
        while (true) {
            
            priority_queue<Candidate> pq;
            for (int v = 0; v < n; ++v) {
                if (!visited[v]) continue;
                for (int e : site.adj[v]) {
                    const Passage& p = site.passages[e];
                    if (!p.active || visited[p.to]) continue;      
                    if (!hasAll(p.requires_, artifacts)) continue; 
                    const Chamber& c = site.chambers[p.to];
                    if (c.cost > remaining) continue;             
                    pq.push({(double)c.value / max(c.cost, 1), c.value, c.id});
                }
            }
            if (pq.empty()) break;

            
            int best = pq.top().chamber;
            visit(best, false);
            ++step;
            if (verbose) {
                const Chamber& c = site.chambers[best];
                cout << "  Step " << step << ": explore \"" << c.name
                     << "\"  cost=" << c.cost << " value=" << c.value
                     << " ratio=" << fixed << setprecision(2)
                     << (double)c.value / max(c.cost, 1)
                     << "  | budget left=" << remaining;
                if (!c.grants.empty()) cout << "  | found: " << join(c.grants);
                cout << "\n";
            }

            
            for (const auto& ev : events) {
                if (ev.trigger != best) continue;
                site.setActive(ev.passage, ev.activate);
                if (verbose)
                    cout << "      >> EVENT: " << ev.message << "\n";
            }
        }

        if (verbose) {
            cout << "  Result: value=" << res.totalValue << ", cost="
                 << res.totalCost << "/" << budget << ", chambers explored="
                 << res.order.size() << "\n";
            
            for (int c = 0; c < n; ++c) {
                if (visited[c]) continue;
                cout << "  Not explored: \"" << site.chambers[c].name << "\" - ";
                bool anyEdge = false, shown = false;
                for (int v = 0; v < n && !shown; ++v) {
                    if (!visited[v]) continue;
                    for (int e : site.adj[v]) {
                        const Passage& p = site.passages[e];
                        if (p.to != c) continue;
                        anyEdge = true;
                        if (!p.active) { cout << "passage hidden/sealed\n"; shown = true; break; }
                        if (!hasAll(p.requires_, artifacts)) {
                            cout << "locked (needs " << join(p.requires_) << ")\n";
                            shown = true; break;
                        }
                        if (site.chambers[c].cost > remaining) {
                            cout << "over budget (cost " << site.chambers[c].cost
                                 << ", left " << remaining << ")\n";
                            shown = true; break;
                        }
                    }
                }
                if (!shown) cout << (anyEdge ? "unreachable\n" : "no route from explored area\n");
            }
        }
        return res;
    }
};



class BruteForce {
    const Site& site;
    int best = 0;

    void dfs(vector<bool>& visited, unordered_set<string>& artifacts,
             int remaining, int value) {
        best = max(best, value);
        int n = (int)site.chambers.size();
        for (int v = 0; v < n; ++v) {
            if (!visited[v]) continue;
            for (int e : site.adj[v]) {
                const Passage& p = site.passages[e];
                if (!p.active || visited[p.to]) continue;
                if (!hasAll(p.requires_, artifacts)) continue;
                const Chamber& c = site.chambers[p.to];
                if (c.cost > remaining) continue;

                visited[c.id] = true;
                vector<string> added;
                for (const auto& a : c.grants)
                    if (artifacts.insert(a).second) added.push_back(a);

                dfs(visited, artifacts, remaining - c.cost, value + c.value);

                for (const auto& a : added) artifacts.erase(a);
                visited[c.id] = false;
            }
        }
    }

public:
    explicit BruteForce(const Site& s) : site(s) {}
    int solve(int start, int budget) {
        vector<bool> visited(site.chambers.size(), false);
        unordered_set<string> artifacts;
        visited[start] = true;
        for (const auto& a : site.chambers[start].grants) artifacts.insert(a);
        best = 0;
        dfs(visited, artifacts, budget, 0);
        return best;
    }
};


static void banner(const string& t) {
    cout << "\n==============================================================\n"
         << t << "\n"
         << "==============================================================\n";
}


static void scenarioLocked() {
    banner("TEST 1: Locked chambers (artifact prerequisites)");
    Site s;
    int gate   = s.addChamber("Entrance", 0, 0);
    int store  = s.addChamber("Storeroom", 2, 6, {"Bronze Key"});
    int vault  = s.addChamber("Treasure Vault", 4, 40);
    int altar  = s.addChamber("Altar Room", 3, 10);
    s.addPassage(gate, store);
    s.addPassage(gate, altar);
    s.addPassage(store, vault, {"Bronze Key"});
    Planner::plan(s, gate, 15);
}


static void scenarioReveal() {
    banner("TEST 2: Passage revealed mid-exploration");
    Site s;
    int gate  = s.addChamber("Entrance", 0, 0);
    int hall  = s.addChamber("Scribe Hall", 3, 12, {"Scroll"});
    int pit   = s.addChamber("Dusty Pit", 2, 4);
    int shrine= s.addChamber("Hidden Shrine", 3, 30);
    s.addPassage(gate, hall);
    s.addPassage(gate, pit);
    int secret = s.addPassage(hall, shrine, {}, 
    vector<Event> ev = {{hall, secret, true,
                         "Scroll decoded - secret door to Hidden Shrine revealed!"}};
    Planner::plan(s, gate, 12, ev);
}


static void scenarioSeal() {
    banner("TEST 3: Passage sealed mid-exploration (alternate route)");
    Site s;
    int gate   = s.addChamber("Entrance", 0, 0);
    int store  = s.addChamber("Storeroom", 2, 9);
    int side   = s.addChamber("Side Passage Hall", 3, 7);
    int treasury = s.addChamber("Royal Treasury", 4, 28);
    s.addPassage(gate, store);
    s.addPassage(gate, side);
    int direct = s.addPassage(store, treasury);   
    s.addPassage(side, treasury);                 
    vector<Event> ev = {{store, direct, false,
                         "Ceiling unstable - tunnel Storeroom -> Royal Treasury SEALED!"}};
    Planner::plan(s, gate, 14, ev);
}


static void scenarioFull() {
    banner("TEST 4: Full expedition (locks + reveals + seals)");
    Site s;
    int gate    = s.addChamber("Entrance", 0, 0);
    int store   = s.addChamber("Storeroom", 2, 6, {"Bronze Key"});
    int hall    = s.addChamber("Scribe Hall", 3, 12, {"Scroll"});
    int vault   = s.addChamber("Treasure Vault", 4, 40);
    int burial  = s.addChamber("Burial Chamber", 5, 50);
    int shrine  = s.addChamber("Hidden Shrine", 3, 30);
    int collapse= s.addChamber("Collapsing Gallery", 2, 8);
    int treasury= s.addChamber("Royal Treasury", 4, 25);

    s.addPassage(gate, store);
    s.addPassage(gate, hall);
    s.addPassage(gate, collapse);
    s.addPassage(store, vault, {"Bronze Key"});
    s.addPassage(hall, burial, {"Scroll"});
    int secret = s.addPassage(hall, shrine, {}, false);
    int shortcut = s.addPassage(store, treasury);
    s.addPassage(collapse, treasury);

    vector<Event> ev = {
        {hall,  secret,   true,  "Secret door to Hidden Shrine revealed!"},
        {store, shortcut, false, "Storeroom -> Royal Treasury tunnel SEALED for safety!"},
    };
    Planner::plan(s, gate, 22, ev);
}


static void scenarioBaseline() {
    banner("TEST 5: Greedy planner vs brute-force baseline (static sites)");

    struct Case { string name; Site site; int budget; };
    vector<Case> cases;

    {   
        Site s;
        int g = s.addChamber("Entrance", 0, 0);
        int a = s.addChamber("A", 2, 10, {"k1"});
        int b = s.addChamber("B", 3, 9);
        int c = s.addChamber("C", 4, 30);
        int d = s.addChamber("D", 2, 5);
        s.addPassage(g, a); s.addPassage(g, b); s.addPassage(g, d);
        s.addPassage(a, c, {"k1"});
        cases.push_back({"Case A", s, 9});
    }
    {   
        Site s;
        int g = s.addChamber("Entrance", 0, 0);
        int a = s.addChamber("Efficient-but-bulky", 6, 24);  
        int b = s.addChamber("Medium-1", 5, 19);             
        int c = s.addChamber("Medium-2", 5, 19);             
        s.addPassage(g, a); s.addPassage(g, b); s.addPassage(g, c);
        cases.push_back({"Case B", s, 10});
    }
    {   
        Site s;
        int g = s.addChamber("Entrance", 0, 0);
        int a = s.addChamber("A", 1, 3, {"x"});
        int b = s.addChamber("B", 2, 7, {"y"});
        int c = s.addChamber("C", 3, 20);
        int d = s.addChamber("D", 3, 22);
        int e = s.addChamber("E", 2, 6);
        s.addPassage(g, a); s.addPassage(g, b); s.addPassage(g, e);
        s.addPassage(a, c, {"x"}); s.addPassage(b, d, {"y"});
        s.addPassage(e, c);
        cases.push_back({"Case C", s, 10});
    }

    cout << left << setw(10) << "Case" << setw(10) << "Greedy"
         << setw(14) << "Brute-force" << "Greedy / Optimal\n";
    cout << string(48, '-') << "\n";
    for (auto& cs : cases) {
        int g = Planner::plan(cs.site, 0, cs.budget, {}, false).totalValue;
        int b = BruteForce(cs.site).solve(0, cs.budget);
        cout << left << setw(10) << cs.name << setw(10) << g << setw(14) << b
             << fixed << setprecision(1) << (b ? 100.0 * g / b : 100.0) << "%\n";
    }
}

int main() {
    cout << "INTELLIGENT ARCHAEOLOGICAL EXPEDITION PLANNER\n"
         << "Greedy best-first exploration on a dynamic chamber graph\n";
    scenarioLocked();
    scenarioReveal();
    scenarioSeal();
    scenarioFull();
    scenarioBaseline();
    cout << "\nDone.\n";
    return 0;
}
