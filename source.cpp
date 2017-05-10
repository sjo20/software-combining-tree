#include<stdio.h>
#include<thread>
#include<atomic>
#include<mutex>
#include<time.h>
#include<vector>
#include<ostream>
#include<condition_variable>
using namespace std;
#define POW_NUM_THREADS 3
#define REP 100000000
enum CStatus {
	IDLE,
	FIRST,
	SECOND,
	RESULT,
	ROOT
};
class Node {
friend   class Combining_Tree;
public:
	bool locked;
	enum CStatus cStatus;
	int id, firstValue, secondValue, result;
	Node* parent;
	condition_variable cv;
	mutex lock;

	Node::Node() : locked(false), cStatus(ROOT), firstValue(0), secondValue(0), result(0), parent(NULL) {}
	Node::Node(Node * myParent) : locked(false), cStatus(IDLE), firstValue(0), secondValue(0), result(0), parent(myParent) {}
	Node *Node::getParent() { return parent; }

	bool Node::precombine() {
		unique_lock<mutex> l(lock);
		while (locked) cv.wait(l);
		switch (cStatus) {
		case IDLE:
			cStatus = FIRST;
			return true;
		case FIRST:
			locked = true;
			cStatus = SECOND;
			return false;
		case ROOT:
			return false;
		default:
			printf("error in precombine");
			exit(1);
		}
	}

	int Node::combine(int combined) {
		unique_lock<mutex> l(lock);
		while (locked) cv.wait(l);
		firstValue = combined;
		locked = true;
		switch (cStatus) {
		case FIRST:
			return  firstValue;
		case SECOND:
			return  firstValue + secondValue;
		default:
			printf("error in combine");
			exit(1);
		}
	}

	int Node::operation(int combined) {
		unique_lock<mutex> l(lock);
		int oldValue;
		switch (cStatus) {
		case ROOT:
			oldValue = result;
			result += combined;
			return oldValue;
		case SECOND:
			secondValue = combined;
			locked = false;
			cv.notify_all();
			while (cStatus != RESULT) cv.wait(l);
			locked = false;
			cv.notify_all();
			cStatus = IDLE;
			return result;
		default:
			printf("error in operation");
			exit(1);
		}
	}

	void Node::distribute(int prior) {
		unique_lock<mutex> l(lock);
		switch (cStatus) {
		case FIRST:
			cStatus = IDLE;
			locked = false;
			break;
		case SECOND:
			result = prior + firstValue;
			cStatus = RESULT;
			break;
		default:
			printf("error in distribute");
			exit(1);
		}
		cv.notify_all();
	}
};
class Combining_Tree {
public:
	Node **leaf, **nodes;
	int num_tree;
	Combining_Tree::Combining_Tree(int size) : num_tree(size)
	{
		int num_leaf = (num_tree + 1) / 2; 
		leaf = new Node*[num_leaf];
		nodes = new Node*[num_tree];
		nodes[0] = new Node();
		nodes[0]->id = 1;
		for (int i = 1; i < num_tree; i++) {
			nodes[i] = new Node(nodes[(i - 1) / 2]);
			nodes[i]->id = i + 1;
		}

		for (int i = 0; i < num_leaf; i++) {
			leaf[i] = nodes[num_tree - num_leaf + i];
		}
	}
	Combining_Tree::~Combining_Tree() {
		for (int i = 0; i < num_tree; i++) {
			delete nodes[i];
		}
		delete[] leaf;
		delete[] nodes;
	}
	int Combining_Tree::getResult() {
		return nodes[0]->result;
	}
	int Combining_Tree::getAndIncrement(int thread_id) {
		Node *myLeaf = leaf[thread_id / 2];

		Node *node = myLeaf;
		while (node->precombine()) {
			node = node->getParent();
		}
		Node *stop = node;
		node = myLeaf;

		int combined = 3;
		vector<Node*> s;
		while (node != stop) {
			combined = node->combine(combined);
			s.push_back(node);
			node = node->getParent(); 
		}

		int prior = stop->operation(combined);
		while (!s.empty()) { 
			node = s.back();
			s.pop_back();
			node->distribute(prior); 
		}
		return prior;
	}
};

Combining_Tree *tree;
void increase(int id) {
	while (tree->getResult() <= REP) {
		tree->getAndIncrement(id);
		//printf("%d ", tree->getResult());
		//_sleep(1000);
		//puts("");
	}
}
int main() {
	for(int i=1;i<=POW_NUM_THREADS;i++) {
		int num_threads = 1 << i;
		vector<thread> t;
		clock_t b, e;

		tree = new Combining_Tree(num_threads - 1);
		b = clock();
		for (int i = 0; i < num_threads; i++) {
			t.push_back(std::thread(increase, i));
		}
		for (auto &th : t)
			th.join();
		e = clock();
		printf("number of threads: %d\n", num_threads);
		printf("elapsed time: %.2f\nresult: %d\n\n", (double)(e - b) / CLOCKS_PER_SEC, tree->getResult());
		t.clear();
		delete tree;
	}
	return 0;
}