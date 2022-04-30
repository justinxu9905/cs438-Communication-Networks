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

void updateForwardTable(set<int> nodes, map<int, map<int, int> > costMat, map<int, map<int, pair<int, int> > > * forwardTable) {
    int n = nodes.size();
    map<int, map<int, int> > dist;
    map<int, bool> visited;
    map<int, int> pre;
    int src, dst, min, minCost, tmp;
    // update forward table for every source node
    for (auto i = nodes.begin(); i != nodes.end(); i++) {
        src = *i;
        // initialization for source node i
        queue<int> topoOrderedNodes;
        for (auto i = nodes.begin(); i != nodes.end(); i++) {
            dist[src][*i] = costMat[src][*i];
            visited[*i] = false;
            pre[*i] = src;
        }
        pre[src] = src;
        visited[src] = true;
        for (int c = 1; c < n; c++) {
            // select next min node
            min = -1;
            minCost = INT32_MAX;
            for (auto j = nodes.begin(); j != nodes.end(); j++) {
                tmp = *j;
                if (!visited[tmp] && dist[src][tmp] >= 0 && (dist[src][tmp] < minCost || (dist[src][tmp] == minCost && tmp < min))) {
                    min = tmp;
                    minCost = dist[src][tmp];
                }
            }
            visited[min] = true;
            topoOrderedNodes.push(min);

            // update distance from source node i to min node's adjacent nodes
            for (auto j = nodes.begin(); j != nodes.end(); j++) {
                dst = *j;
                if (!visited[dst] && costMat[min][dst] >= 0 &&
                    (minCost + costMat[min][dst] < dist[src][dst]
                    || (minCost + costMat[min][dst] == dist[src][dst] && min < pre[dst])
                    || dist[src][dst] < 0)
                ) {
                    dist[src][dst] = minCost + costMat[min][dst];
                    pre[dst] = min;
                }
//cout << src << " to " << dst << "'s pre: " << pre[dst] << endl;
            }
        }

        while (!topoOrderedNodes.empty()) {
            dst = topoOrderedNodes.front();
            topoOrderedNodes.pop();
            if (pre[dst] == src) {
                (*forwardTable)[src][dst] = make_pair(dst, dist[src][dst]);
            } else {
                (*forwardTable)[src][dst] = make_pair((*forwardTable)[src][pre[dst]].first, dist[src][dst]);
            }
        }
        /*for (auto j = nodes.begin(); j != nodes.end(); j++) {
            dst = *j;
            cout << src << " -> " << dst << " distance: " << dist[src][dst] << endl;
            cout << "pre: " << pre[dst] << endl;
        }*/
        /*for (auto j = nodes.begin(); j != nodes.end(); j++) {
            dst = *j;
            cout << src << " -> " << dst << " distance: " << (*forwardTable)[src][dst].second << " via " << (*forwardTable)[src][dst].first << endl;
            cout << "pre: " << pre[dst] << endl;
        }*/

    }
    // cout << endl;
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
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
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

