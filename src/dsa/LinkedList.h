#include <stdexcept>
#include <utility>

template <class T>
DoublyLinkedList<T>::Node::Node(const T& value)
    : data(value) {
}

template <class T> DoublyLinkedList<T>::DoublyLinkedList() = default;

template <class T> DoublyLinkedList<T>::DoublyLinkedList(const DoublyLinkedList& other) {
    try {
        other.forEach([this](const T& value) {
            pushBack(value);
        });
    } catch (...) {
        clear();
        throw;
    }
}

template <class T>
DoublyLinkedList<T>& DoublyLinkedList<T>::operator=(const DoublyLinkedList& other) {
    if (this != &other) {
        DoublyLinkedList copy(other);
        swap(copy);
    }
    return *this;
}

template <class T> DoublyLinkedList<T>::~DoublyLinkedList() {
    clear();
}

template <class T> void DoublyLinkedList<T>::unlink(Node* node) noexcept {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        head_ = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        tail_ = node->prev;
    }
    delete node;
    --count_;
}

template <class T> void DoublyLinkedList<T>::swap(DoublyLinkedList& other) noexcept {
    using std::swap;
    swap(head_, other.head_);
    swap(tail_, other.tail_);
    swap(count_, other.count_);
}

template <class T> bool DoublyLinkedList<T>::empty() const noexcept {
    return count_ == 0;
}

template <class T> std::size_t DoublyLinkedList<T>::size() const noexcept {
    return count_;
}

template <class T> T& DoublyLinkedList<T>::front() {
    if (!head_) {
        throw std::out_of_range("empty list");
    }
    return head_->data;
}

template <class T> const T& DoublyLinkedList<T>::front() const {
    if (!head_) {
        throw std::out_of_range("empty list");
    }
    return head_->data;
}

template <class T> T& DoublyLinkedList<T>::back() {
    if (!tail_) {
        throw std::out_of_range("empty list");
    }
    return tail_->data;
}

template <class T> const T& DoublyLinkedList<T>::back() const {
    if (!tail_) {
        throw std::out_of_range("empty list");
    }
    return tail_->data;
}

template <class T> void DoublyLinkedList<T>::pushFront(const T& value) {
    Node* node = new Node(value);
    node->next = head_;
    if (head_) {
        head_->prev = node;
    } else {
        tail_ = node;
    }
    head_ = node;
    ++count_;
}

template <class T> void DoublyLinkedList<T>::pushBack(const T& value) {
    Node* node = new Node(value);
    node->prev = tail_;
    if (tail_) {
        tail_->next = node;
    } else {
        head_ = node;
    }
    tail_ = node;
    ++count_;
}

template <class T> bool DoublyLinkedList<T>::popFront() {
    if (!head_) {
        return false;
    }
    unlink(head_);
    return true;
}

template <class T> bool DoublyLinkedList<T>::popBack() {
    if (!tail_) {
        return false;
    }
    unlink(tail_);
    return true;
}

template <class T> template <class F> void DoublyLinkedList<T>::forEach(F callback) const {
    for (const Node* node = head_; node; node = node->next) {
        callback(node->data);
    }
}

template <class T> template <class F> void DoublyLinkedList<T>::forEachReverse(F callback) const {
    for (const Node* node = tail_; node; node = node->prev) {
        callback(node->data);
    }
}

template <class T> template <class F> std::size_t DoublyLinkedList<T>::eraseIf(F predicate) {
    std::size_t erased = 0;
    for (Node* node = head_; node;) {
        Node* next = node->next;
        if (predicate(node->data)) {
            unlink(node);
            ++erased;
        }
        node = next;
    }
    return erased;
}

template <class T> void DoublyLinkedList<T>::clear() noexcept {
    while (head_) {
        unlink(head_);
    }
}
