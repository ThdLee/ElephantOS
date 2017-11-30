#include "list.h"
#include "interrupt.h"

void list_init(struct list* plist) {
	plist->head.prev = NULL;
	plist->head.next = &plist->tail;
	plist->tail.prev = &plist->head;
	plist->tail.next = NULL;
}

void list_insert_before(struct list_elem* before, struct list_elem* elem) {
	enum intr_status old_status = intr_disable();

	before->prev->next = elem;
	elem->next = before;
	elem->prev = before->prev;
	before->prev = elem;

	intr_set_status(old_status);
}

void list_push(struct list* plist, struct list_elem* elem) {
	list_insert_before(plist->head.next, elem);
}

void list_append(struct list* plist, struct list_elem* elem) {
	list_insert_before(&plist->tail, elem);
}

void list_remove(struct list_elem* elem) {
	enum intr_status old_status = intr_disable();

	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;

	intr_set_status(old_status);
}

struct list_elem* list_pop(struct list* plist) {
	struct list_elem* elem = plist->head.next;
	list_remove(elem);
	return elem;
}

bool elem_find(struct list* plist, struct list_elem* obj_elem) {
	struct list_elem* elem = plist->head.next;
	while(elem != &plist->tail) {
		if (elem == obj_elem) {
			return true;
		}
		elem = elem->next;
	}
	return false;
}

/* 把链表plist中的每个元素和arg传给回调函数func，
 * arg给func用来判断elem是否符合条件。
 * 本函数的功能是遍历链表内所有元素，逐个判断是否有符合条件的元素。
 * 找到则返回元素指针，否则返回NULL。 */
struct list_elem* list_traversal(struct list* plist, function func, int arg) {
	struct list_elem* elem = plist->head.next;

	if (list_empty(plist)) {
		return NULL;
	}

	while (elem != &plist->tail) {
		if (func(elem, arg)) {
			return elem;
		}
		elem = elem->next;
	}
	return NULL;
}

uint32_t list_len(struct list* plist) {
	struct list_elem* elem = plist->head.next;
	uint32_t length = 0;
	while (elem != &plist->tail) {
		length++;
		elem = elem->next;
	}
	return length;
}

bool list_empty(struct list* plist) {
	return plist->head.next == &plist->tail;
}