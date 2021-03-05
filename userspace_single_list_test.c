////////////////////////////////////////////
//sing list
////////////////////////////////////////////


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/*  */
struct cell{
    struct cell *next;
    int data;
};
typedef struct cell cell_t;

/*!
 * @brief       新規セルを生成する。
 * @param[in]   data  データ
 * @return      新規セルのアドレスを返す。失敗の場合はNULLを返す。
 */
cell_t *list_alloc(int data)
{
    cell_t *new = NULL;

    new = (cell_t *)malloc(sizeof(cell_t));
    if(new == NULL){
        fprintf(stderr, "ERROR: list_alloc(): %s\n", strerror(errno));
        return(NULL);
    }

    new->next = NULL;
    new->data = data;

    return(new);
}

/*!
 * @brief      リスト末尾に新規セルを追加する
 * @param[in]  header  リストの先頭要素
 * @param[in]  data  データ
 */
int list_add(cell_t *header, int data)
{
    cell_t *next = NULL;
    cell_t *prev = header;

    /* セルを生成する */
    next = list_alloc(data);
    if(next == NULL) return(-1);

    /* 末尾にセルを追加する */
    while(prev->next != NULL){
        prev = prev->next;
    }
    prev->next = next;

    return(0);
}

/*!
 * @brief      リストの要素を全て解放する
 * @param[in]  header  リストの先頭要素
 */
void list_free(cell_t *header)
{
    cell_t *temp = header;
    cell_t *swap = NULL;

    while(temp != NULL){
        swap = temp->next;
        free(temp);
        temp = swap;
    }
}

/*!
 * @brief      リストの要素を一覧表示する
 * @param[in]  header  リストの先頭要素
 */
static void list_print(cell_t *header)
{
    cell_t *p = header;

    printf("list{ ");
    while(p != NULL){
        printf("%d ", p->data);
        p = p->next;
    }
    printf("}\n");
}

int main(void)
{
    int cnt = 0;

    /* 先頭セルを用意する */
    cell_t *header = list_alloc(0);
    if(header == NULL)
        return(-1);

    /* 要素を追加する */
    for(cnt = 1; cnt < 10; cnt++){
        list_add(header, cnt);
    }
    list_print(header);

    /* リストを解放する */
    list_free(header);

    return(0);
}
