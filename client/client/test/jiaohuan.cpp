#include<iostream>
  //Definition for singly-linked list.
  struct ListNode {
      int val;
      ListNode *next;
      ListNode() : val(0), next(nullptr) {}
      ListNode(int x) : val(x), next(nullptr) {}
      ListNode(int x, ListNode *next) : val(x), next(next) {}
  };

 //时间和空间复杂度都为0(n)操作次数为n/2
class Solution {
public:
    ListNode* swapPairs(ListNode* head) {
        //1首先明确递归的终止条件，就是首先得传递到最后一个节点
        if (head == nullptr || head->next == nullptr)
        {
            return head;
        }
        // newHead 是当前组的第二个节点，交换后会变成新的头节点
        ListNode* newHead = head->next;
        //2假设后面的节点已经交换好了,使头节点指向后续已经交换好的改变的头节点，newhead变成新的头节点
        head->next = swapPairs(newHead->next);
        newHead->next = head;
        return newHead;//返回当前组的新的头节点
    }
};
class Solution2
{
public:
    ListNode*swapPairs(ListNode* head)
    {
        //1首先就是递归的终止条件
        if (head == nullptr || head->next == nullptr)
        {
            return head;
        }
        //2定义一个当前组的第二个节点
        ListNode* newHead = head->next;
        //3使得
        head->next = swapPairs(newHead->next);
        newHead->next = head;
        return newHead;
    }
};
int main()
{
    ListNode* l1 = new ListNode(1);
    l1->next = new ListNode(2);
    l1->next->next = new ListNode(3);
    l1->next->next->next = new ListNode(4);
    Solution2 s2;
    auto l2= s2.swapPairs(l1);
    if (l2->val == 2)
    {
        std::cout << "hello" << std::endl;
    }
   return 0;
}