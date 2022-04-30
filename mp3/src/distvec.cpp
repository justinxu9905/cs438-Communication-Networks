#include<bits/stdc++.h>
#include<map>

using namespace std;

typedef struct message {
    int src;
    int dst;
    string msg;
    message(int src, int dst, string msg) : src(src), dst(dst), msg(msg) {}
} message;

ofstream fpOut;

void loadMessages(string filename, vector<message> * messageVector) {
    ifstream is;
    is.open(filename, ios::in);

    string line, msg;
    if (is.is_open()) {
        while (getline(is, line)) {
            if (line.empty()) {
                continue;
            }
            int src, dst;
            sscanf(line.c_str(), "%d %d %*s", &src, &dst);
            msg = line.substr(line.find(" "));
            msg = msg.substr(line.find(" ") + 1);
            messageVector->push_back(message(src, dst, msg));
        }
    }

    is.close();
}

void init(string filename, set<int> * nodes, map<int, map<int, int> > * dist) {
    ifstream is;
    is.open(filename, ios::in);

    int src, dst, cost;
    while (is >> src >> dst >> cost) {
        (*dist)[src][dst] = cost;
        (*dist)[dst][src] = cost;
        if (nodes->find(src) == nodes->end()) {
            nodes->insert(src);
        }
        if (nodes->find(dst) == nodes->end()) {
            nodes->insert(dst);
        }
    }

    for (auto i = nodes->begin(); i != nodes->end(); i++) {
        src = *i;
        for (auto j = nodes->begin(); j != nodes->end(); j++) {
            dst = *j;
            if (src == dst) {
                (*dist)[src][dst] = 0;
            } else if ((*dist)[src].find(dst) == (*dist)[src].end()) {
                (*dist)[src][dst] = -999;
            }
            // cout << src << " " << dst << " " << dist[src][dst] << endl;
        }
    }

    is.close();
}

void initForwardTable(set<int>nodes, map<int, map<int, int> > dist, map<int, map<int, pair<int, int> > > * forwardTable) {
    int src, dst;
    for (auto i = nodes.begin(); i != nodes.end(); i++) {
        src = *i;
        for (auto j = nodes.begin(); j != nodes.end(); j++) {
            dst = *j;
            (*forwardTable)[src][dst] = make_pair(dst, dist[src][dst]);
        }
    }
}

void updateForwardTable(set<int> nodes, map<int, map<int, int> > dist, map<int, map<int, pair<int, int> > > * forwardTable) {
    int n = nodes.size();
    int src, dst, min, minCost;
    for (int c = 0; c < n; c++) {
        for (auto i = nodes.begin(); i != nodes.end(); i++) {
            src = *i;
            for (auto j = nodes.begin(); j != nodes.end(); j++) {
                dst = *j;
                min = (*forwardTable)[src][dst].first;
                minCost = (*forwardTable)[src][dst].second;
                for (auto v = nodes.begin(); v != nodes.end(); v++) {
                    if (dist[src][*v] >= 0 && (*forwardTable)[*v][dst].second >= 0) {
                        if (minCost < 0 || dist[src][*v] + (*forwardTable)[*v][dst].second < minCost || (dist[src][*v] + (*forwardTable)[*v][dst].second == minCost && *v < min && src != *v)) {
                            min = *v;
                            minCost = dist[src][*v] + (*forwardTable)[*v][dst].second;
                        }
                    }
                }
                (*forwardTable)[src][dst] = make_pair(min, minCost);
            }
        }
    }
    /*for (auto i = nodes.begin(); i != nodes.end(); i++) {
        src = *i;
        for (auto j = nodes.begin(); j != nodes.end(); j++) {
            dst = *j;
            cout << src << " -> " << dst << " distance: " << (*forwardTable)[src][dst].second << " via " << (*forwardTable)[src][dst].first << endl;
        }
    }*/
}

void handleOutput(vector<message> messageVector, set<int> nodes, map<int, map<int, pair<int, int> > > forwardTable) {
    int src, dst, cost, nextHop;
    for (auto i = nodes.begin(); i != nodes.end(); i++) {
        src = *i;
        for (auto j = nodes.begin(); j != nodes.end(); j++) {
            dst = *j;
            if (forwardTable[src][dst].second < 0) {
                continue;
            }
            fpOut << dst << " " << forwardTable[src][dst].first << " " << forwardTable[src][dst].second << endl;
        }
        fpOut << endl;
    }
    for (int i = 0; i < messageVector.size(); ++i) {
        src = messageVector[i].src;
        dst = messageVector[i].dst;
        nextHop = src;
        fpOut << "from " << src << " to " << dst << " cost ";
        cost = forwardTable[src][dst].second;
        if (cost == -999) {
            fpOut << "infinite hops unreachable ";
        } else {
            fpOut << cost << " hops ";
            while (nextHop != dst) {
                fpOut << nextHop << " ";
                nextHop = forwardTable[nextHop][dst].first;
            }
        }
        fpOut << "message" << messageVector[i].msg << endl;
        fpOut << endl;
    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    string topoFilename = argv[1];
    string messageFilename = argv[2];
    string changesFilename = argv[3];

    fpOut.open("output.txt");

    vector<message> messageVector;
    set<int> nodes;
    map<int, map<int, int> > dist;
    map<int, map<int, pair<int, int> > > forwardTable;

    loadMessages(messageFilename, &messageVector);

    init(topoFilename, &nodes, &dist);
    initForwardTable(nodes, dist, &forwardTable);
    updateForwardTable(nodes, dist, &forwardTable);

    handleOutput(messageVector, nodes, forwardTable);

    ifstream changesIs;
    changesIs.open(changesFilename, ios::in);
    int src, dst, cost;
    if (changesIs.is_open()) {
        while (changesIs >> src >> dst >> cost) {
            dist[src][dst] = cost;
            dist[dst][src] = cost; // init distance matrix again
            initForwardTable(nodes, dist, &forwardTable);
            updateForwardTable(nodes, dist, &forwardTable);

            handleOutput(messageVector, nodes, forwardTable);
        }
    }
    changesIs.close();


    fpOut.close();

    return 0;
}

