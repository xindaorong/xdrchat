#include <iostream>
using namespace std;
 struct ListNode {
      int val;
      ListNode *next;
      ListNode() : val(0), next(nullptr) {}
      ListNode(int x) : val(x), next(nullptr) {}
      ListNode(int x, ListNode *next) : val(x), next(next) {}
  };

int main()
{

    //l1 和 l2 是两个不同的指针变量；
//但它们指向同一个节点；
//所以通过 l1->next 修改节点内容后，l2->next 也能看到同样的变化。
   ListNode* l1 = new ListNode(1);
    l1->next = new ListNode(2);
    l1->next->next = new ListNode(3);
   ListNode* tail = l1->next->next;
   
   ListNode* l2 = l1;
   

   
  //这个代表指针l1和l2在内存中的位置
   std::cout<<&l1<<std::endl;
   std::cout<<&l2<<std::endl;
   //这个代表真实节点在内存中的位置，就是l1和l2指向的节点在内存中的位置
   std::cout<<l1<<std::endl;
   std::cout<<l2<<std::endl;


   l1->next = tail;

   std::cout<<l1->next<<std::endl;
   std::cout<<l2->next<<std::endl;
   std::cout<<&(l1->next)<<std::endl;
   std::cout<<&(l2->next)<<std::endl;
}