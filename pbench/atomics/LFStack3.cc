#include <atomic>
 
template<class T>
struct node
{
    T data;
    node* next;
    node(const T& data) : data(data), next(nullptr) {}
};
 
template<class T>
class stack
{    
 public:
    std::atomic<node<T>*> head;
    std::atomic<int> push_lock; 
    void push(const T& data)
    {
        node<T>* new_node = new node<T>(data);    
        int expected = 0;
        //Acquiring Lock
        while (!push_lock.compare_exchange_weak(expected, 1, std::memory_order_acquire));
        new_node->next = head;
        head = new_node;
        // Unlock A
        push_lock.store(0, std::memory_order_release);      

    }
};

int main()
{
    stack<int> s;
    s.push(1);

}

// //  /https://en.cppreference.com/w/cpp/atomic/atomic_compare_exchange