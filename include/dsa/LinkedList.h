#pragma once
#include <stdexcept>

template<typename T>
class LinkedList{
    private:
        struct Node{
            T data;
            Node* next;

            Node(const T& value) : data(value), next(nullptr) {}
        };
        Node* head;
        Node* tail;
        int count;
        Node* split(Node* start){
            Node* slow = start;
            Node* fast = start->next;
            while(fast != nullptr && fast->next!=nullptr){
                slow = slow->next;
                fast= fast->next->next;
            }
            Node* right = slow->next;
            slow->next = nullptr;
            return right;
        }
        Node* merge(Node* left, Node* right, bool (*less)(const T&, const T&)){
            Node* result = nullptr;
            Node* last = nullptr;
            while(left!= nullptr && right != nullptr){
                Node* chosen;
                if(!less(right->data,left->data)){
                    chosen = left;
                    left = left->next;
                }
                else{
                    chosen = right;
                    right = right->next;
                }
                if(result == nullptr) result = last = chosen;
                else{
                    last->next = chosen;
                    last = chosen;
                }
            }
            if(last != nullptr){ //van hoi kho hieu
                last->next = (left!=nullptr) ? left : right;
            }
            return result;
        }
        Node* mergeSortNodes(Node* start, bool (*less)(const T&, const T&)){
            if(start == nullptr || start->next == nullptr) return start;
            Node* right = split(start);
            Node* leftSorted = mergeSortNodes(start, less);
            Node* rightSorted = mergeSortNodes(right, less);
            return merge(leftSorted, rightSorted, less);
        }
    public:
        LinkedList() : head(nullptr), tail(nullptr), count(0) {};
        LinkedList(const LinkedList& other) : LinkedList(){
            Node* current = other.head;
            try{
                while(current != nullptr){
                    pushBack(current->data);
                    current = current->next;
                }
            }
            catch(...){
                clear();
                throw;
            }
        };
        LinkedList& operator=(const LinkedList& other){
            if(this==other&) return this*;
            clear();
            Node* current = other.head;
            while(current != nullptr){
                pushBack(current->data);
                current = current->next;
            }
            return *this;
        }
        ~LinkedList(){
            clear();
        }
        void pushBack(const T& data){
            Node* node = new Node(data);
            if(this->head == nullptr) this->head=this->tail=node;
            else{
                tail->next=node;
                tail=node;
            }
            count++;
        }
        void pushFront(const T& data){
            Node* node= new Node(data);
            node->next=this->head;
            this->head=node;
            if(tail==nullptr) tail = node;
            count++;
        }
        bool removeAt(int index){
            if(index < 0 || index >= count) return false;
            Node* removed;
            if(index==0){
                removed = this->head;
                this->head = this->head->next;
                this->tail=nullptr;
            }
            else{
                Node* previous = this->head;
                for(int i = 0; i < index -1; i++){
                    previous = previous->next;
                }
                removed = previous->next;
                previous->next=removed->next;
                if(tail==removed) tail = previous;
            }
            delete removed;
            count--;
            return true;
        }
        T& at(int index){
            if(index < 0 || index >= count) throw std::out_of_range("Invalid index");
            Node* current = this->head;
            for(int i = 0; i < index; i++) current = current->next;
            return current->data;
        }
        const T& at(int index) const{
            if(index < 0 || index >= count) throw std::out_of_range("Invalid index");
            const Node* current = this->head;
            for(int i = 0; i < index; i++) current = current->next;
            return current->data;
        }
        int size() const{
            return count;
        }
        bool empty() const{
            return count==0;
        }
        void clear(){
            while(head!=nullptr){
                Node* temp = this->head;
                this->head = this->head->next;
                delete temp;
            }
            tail = nullptr;
            count = 0;
        }
        void mergeSort(bool (*less)(const T&, const T&)){
            if(count < 2) return;
            head=mergeSortNodes(head,less);
            tail = head;
            while(tail->next!=nullptr){
                tail=tail->next;
            } 
        }
};