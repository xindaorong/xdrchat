#include<iostream>
#include<vector>
using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode* next) : val(x), next(next) {}

};
//方法一双指针法
class Solution1
{
public:
    bool ishuiwen(ListNode* head)
    {
        //1首先选好容器
        vector<int>vals;

        //2把节点的值插到链表中即可
        while (head)
        {
            vals.emplace_back(head->val);
            head = head->next;
        }

        //3开始判断是否是回文
        for (int i = 0, j = vals.size() - 1; i < j; i++,j--)
        {
            if (vals[i] != vals[j])
            {
                return false;
            }
        }

        //4否则是回文链表
        return true;
    }
};
int main()
{
    Solution1 s1;
    ListNode* head;
    head->val = 1;
    ListNode* head1;
    head->next = head1;
    head1->val = 2;
    ListNode* head2;
    head->next->next = head2;
    head2->val = 1;

    bool ishui = s1.ishuiwen(head);
    if (ishui)
    {
        cout << 1;
    }
    else
    {
        cout << 2;
    }

}