#pragma once
#include <stdexcept>

template<typename T>
class LinkedList{
    private:
        struct Node{
            T data;
            Node* next;

            Node(const T& value) : data(value), next(nullptr) {}
        }
        Node* head;
        Node* tail;
        int count;
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
            catch{
                clear();
                throw;
            }
        };
        LinkedList& operator=(const LinkedList& other){
            if(this=other&) return this*;
            clear();
            Node* current = other.head;
            while(current != nullptr){
                pushBack(current->data)
                current = current->next;
            }
            return this*;
        }
        ~LinkedList(){
            clear();
        }
        void pushBack(const T& data){
            Node* node = new Node(value);
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
            }
            else{
                Node* previous = this->head;
                for(int i = 0; i < index -1; i++){
                    previous = previous->next;
                }
                removed = previous->next;
                previouus->next=removed->next;
                if(tail==removed) tail = previous;
            }
            delete removed;
            cout--;
            return true;
        }
        T& at(int index){
            if(index < 0 || index >= count) throw std::out_of_range("Invalid index");
            Node* current = this->head;
            for(int i = 0; i < index; i++) current = current->next;
            return current->data;
        }
        const T& at(int index) const{
            if(index < 0 || index >= cout) throw std::out_of_range("Invalid index");
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
};