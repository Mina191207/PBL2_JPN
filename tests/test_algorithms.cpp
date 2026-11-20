#include "TestSupport.h"
#include "algorithms/KmpMatcher.h"
#include "algorithms/MergeSort.h"
#include "dsa/LinkedList.h"
#include "IdGenerator.h"
#include "domain/EntityId.h"
#include "domain/QuizSession.h"
#include <limits>
#include <vector>

struct Item {
    static int alive, copiesLeft;
    int key, order;
    Item() = delete;
    Item(int k, int n) : key(k), order(n) { ++alive; }
    Item(const Item& other) : key(other.key), order(other.order) {
        if (copiesLeft == 0) throw std::runtime_error("copy failed");
        if (copiesLeft > 0) --copiesLeft;
        ++alive;
    }
    Item& operator=(const Item&) = default;
    ~Item() { --alive; }
};
int Item::alive = 0;
int Item::copiesLeft = -1;

void listTests() {
    DoublyLinkedList<int> list;
    CHECK(list.empty() && list.size() == 0 && !list.popBack() && !list.popFront());
    throws([&] { list.front(); });
    const auto& constList = list;
    throws([&] { constList.back(); });
    for (int i = 1; i <= 5; ++i) list.pushBack(i);
    list.pushFront(0);
    std::vector<int> forward, reverse;
    list.forEach([&](int i) { forward.push_back(i); });
    list.forEachReverse([&](int i) { reverse.push_back(i); });
    CHECK(forward == std::vector<int>({0,1,2,3,4,5}));
    CHECK(reverse == std::vector<int>({5,4,3,2,1,0}));
    auto copy = list;
    CHECK(list.eraseIf([](int i) { return i >= 1 && i <= 4; }) == 4);
    CHECK(list.size() == 2 && list.front() == 0 && list.back() == 5);
    list = list;
    CHECK(list.size() == 2);
    CHECK(list.popFront() && list.front() == 5 && list.back() == 5);
    CHECK(list.popBack() && list.empty());
    CHECK(copy.size() == 6);
    copy = list;
    CHECK(copy.empty());
    list.pushFront(7);
    CHECK(list.popBack() && list.empty());
    for (int i = 0; i < 4; ++i) list.pushBack(i);
    CHECK(list.eraseIf([](int) { return true; }) == 4 && list.empty());

    DoublyLinkedList<Item> source, target;
    source.pushBack(Item(1,1)); source.pushBack(Item(2,2)); source.pushBack(Item(3,3));
    target.pushFront(Item(9,9));
    const int before = Item::alive;
    Item::copiesLeft = 1;
    throws([&] { DoublyLinkedList<Item> fail(source); });
    CHECK(Item::alive == before && source.size() == 3);
    Item::copiesLeft = 1;
    throws([&] { target = source; });
    CHECK(Item::alive == before && target.size() == 1 && target.front().key == 9);
    Item::copiesLeft = -1;
    target = source;
    source.front().key = 8;
    CHECK(target.front().key == 1);
}
void algorithmTests() {
    throws([] { QuizSession empty({}); });
    for (const std::string pattern : {"", "a", "aba", "ababac", "aaaaab", "long pattern", "学校", "học"}) {
        KmpMatcher matcher(pattern);
        for (const std::string text : {"", "a", "ababababac", "aaaaaaaab", "学校へ学校", "tôi học"})
            CHECK(matcher.findFirst(text) == text.find(pattern));
    }
    std::vector<Item> values;
    mergeSort(values, [](const Item& a, const Item& b) { return a.key < b.key; });
    values.emplace_back(2,0);
    mergeSort(values, [](const Item& a, const Item& b) { return a.key < b.key; });
    values.emplace_back(1,1); values.emplace_back(2,2); values.emplace_back(1,3);
    mergeSort(values, [](const Item& a, const Item& b) { return a.key < b.key; });
    CHECK(values[0].order == 1 && values[1].order == 3 && values[2].order == 0 && values[3].order == 2);
    for (std::uint64_t n : {999ULL, 99999ULL, 999999ULL})
        CHECK(idAfter<ResourceId>(n).number() == n + 1);
    CHECK(idAfter<ResourceId>(0).getId() == "R001");
    CHECK(ResourceId::isValid("R18446744073709551615"));
    CHECK(!ResourceId::isValid("R18446744073709551616"));
    throws([] { idAfter<ResourceId>(std::numeric_limits<std::uint64_t>::max()); });
    for (const auto* invalid : {"R000", "R01", "R0001", "r001", "R+001", "R001x"})
        CHECK(!ResourceId::isValid(invalid));
}
int main() {
    try { listTests(); CHECK(Item::alive == 0); algorithmTests(); CHECK(Item::alive == 0); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
