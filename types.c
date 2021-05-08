#include <windows.h>
#include <stdint.h>
#include <psapi.h>
#include "types.h"


// List Structure Functions
DWORD _stdcall initList(_Out_ List** list) {
    DWORD result = ERROR_SUCCESS;

    *list = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(List));

    if (*list == NULL)
        result = ERROR_NOT_ENOUGH_MEMORY;
    else
        (*list)->next = *list;

    return result;
}

DWORD _stdcall insertElement(_Inout_ List** list, _In_ GObject* element) {
    DWORD result = ERROR_SUCCESS;
    List* aux = *list;

    while (aux->next != *list)
        aux = aux->next;

    aux->next = NULL;
    aux->next = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(List));
    if (aux->next == NULL) {
        result = ERROR_NOT_ENOUGH_MEMORY;
        goto Exit;
    }

    aux = aux->next;
    aux->element = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(GObject));
    aux->next = *list;
    if (aux->element == NULL) {
        result = ERROR_NOT_ENOUGH_MEMORY;
        goto Exit;
    }


    aux->element->screenPosX = element->screenPosX;
    aux->element->screenPosY = element->screenPosY;
    aux->element->existHitbox = element->existHitbox;
    aux->element->sprite.bitmapInfo = element->sprite.bitmapInfo;
    aux->element->sprite.memory = element->sprite.memory;
    aux->element->damage = element->damage;
    aux->element->direction = element->direction;
    aux->element->script = element->script;
    aux->element->canBeDestroyed = element->canBeDestroyed;
Exit:
    return result;
}

DWORD _stdcall destroyElement(_Inout_ List** list) {
    DWORD result = ERROR_SUCCESS;
    List* aux = *list;

    if ((*list)->element) {
        while (aux->next != *list)
            aux = aux->next;

        if ((*list)->element) {
            HeapFree(GetProcessHeap(), NULL, (*list)->element->sprite.memory);
            HeapFree(GetProcessHeap(), NULL, (*list)->element);
            aux->next = (*list)->next;
            HeapFree(GetProcessHeap(), NULL, (*list));
            *list = aux;
        }
        else
            result = ERROR_EMPTY;
    }
    return result;
}

void _stdcall freeList(_In_ List** list) {

    List* aux = (*list)->next;

    while (aux != (*list))
        destroyElement(&aux);

    HeapFree(GetProcessHeap(), NULL, (*list));
}