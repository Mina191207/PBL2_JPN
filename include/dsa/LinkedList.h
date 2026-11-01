#pragma once
#include <cstddef>

template <class T> class DoublyLinkedList {
    struct Node {
        T data;
        Node* prev = nullptr;
        Node* next = nullptr;
        explicit Node(const T& value);
    };

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t count_ = 0;

    void unlink(Node* node) noexcept;

  public:
    DoublyLinkedList();
    DoublyLinkedList(const DoublyLinkedList& other);
    DoublyLinkedList& operator=(const DoublyLinkedList& other);
    ~DoublyLinkedList();
    void swap(DoublyLinkedList& other) noexcept;
    bool empty() const noexcept;
    std::size_t size() const noexcept;
    T& front();
    const T& front() const;
    T& back();
    const T& back() const;
    void pushFront(const T& value);
    void pushBack(const T& value);
    bool popFront();
    bool popBack();
    template <class F> void forEach(F callback) const;
    template <class F> void forEachReverse(F callback) const;
    template <class F> std::size_t eraseIf(F predicate);
    void clear() noexcept;
};

#include "../../src/dsa/LinkedList.h"
