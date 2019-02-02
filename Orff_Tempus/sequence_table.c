/*
Sequence table/顺序表:
	顺序表，全名顺序存储结构，是线性表的一种。
	线性表用于存储逻辑关系为“一对一”的数据，顺序表自然也不例外。
	不仅如此，顺序表对数据的物理存储结构也有要求。
	顺序表存储数据时，会提前申请一整块足够大小的物理空间，然后将数据依次存储起来，存储时做到数据元素之间不留一丝缝隙。
	顺序表存储数据同数组非常接近。其实，顺序表存储数据使用的就是数组。
顺序表的初始化:
	使用顺序表存储数据之前，除了要申请足够大小的物理空间之外，
	为了方便后期使用表中的数据，顺序表还需要实时记录以下 2 项数据：
		1.顺序表申请的存储容量；
		2. 顺序表的长度，也就是表中存储数据元素的个数；
		提示：正常状态下，顺序表申请的存储容量要大于顺序表的长度。
*/

#include <stdio.h>
#include <stdlib.h>

#define SIZE 50			//顺序表申请空间的大小

typedef struct sequence_table{
	int * head;		//声明了一个名为head的长度不确定的数组，也叫"动态数组"。实际数据保存处。
	int length;		//记录当前顺序表的长度
	int size;		//记录顺序表分配的存储容量
} table;

int init_table(table *t)
{
	t->head = (int*)malloc(SIZE * sizeof(int));
	if (!t->head) {
		printf("初始化失败\n");
		exit(-1);
	}
	t->length = 0;
	t->size = SIZE;
	return 0;
}

void display_table(table *t)
{
	for (int i = 0; i < t->length; i++) {
		printf("%d ", t->head[i]);
	}
	printf("\n");
}

int main(void)
{
	table t;	//创建一个空的顺序表
	
	if (!init_table(&t)) {
		//向顺序表中添加元素
		for (int i = 1; i <= SIZE; i++) {
			t.head[i-1] = i-1;
			t.length++;
		}
		
		printf("顺序表中存储的元素分别是：\n");
		display_table(&t);
	}
	return 0;
}
