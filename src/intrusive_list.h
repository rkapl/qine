#pragma once

#include "cpp.h"
#include <iterator>

namespace IntrusiveList {
    template<class T> class DefaultAccessor;

    class GenericNode: private NoMove, NoCopy {
    public:
        GenericNode(): m_next(nullptr), m_prev(nullptr) {}
        GenericNode *m_next;
        GenericNode *m_prev;

        // Used for elements
        bool is_linked() const {
            return !(m_next == nullptr && m_prev == nullptr);
        }

        // Used for heads
        bool is_empty() const {
            return m_next == this;
        }

        void erase() {
            m_prev->m_next = m_next;
            m_next->m_prev = m_prev;
            m_next = nullptr;
            m_prev = nullptr;
        }

        void insert_after(GenericNode *next) {
            if (is_empty()) {
                m_next = next;
                m_prev = next;
                next->m_next = this;
                next->m_prev = this;
            } else {
                auto next2 = m_next;
                m_next = next;
                next2->m_prev = next;
                next->m_prev = this;
                next->m_next = next2;
            }
        }

        void clear() {
            while(!is_empty()) {
                m_next->erase();
            }
        }
    };

    class DefaultMarker;

    template <class M=DefaultMarker>
    class Node: public GenericNode {
    public:
        ~Node() {
            if (is_linked())
                erase();
        }
    };

    /**
     * Generally useful for unsorted pool of objects, which we want to keep e.g. for debugging or notifications. 
     * 
     * T is the stored type.
     * A is the accessor type trait that must provide to_node and from_node.
     */
    template<class T, class M=DefaultMarker>
    class List: private NoMove, private NoCopy {
    public:
        class Iterator;

        List() {
            m_head.m_next = &m_head;
            m_head.m_prev = &m_head;
        }
        ~List() {
            m_head.clear();
        }

        void add_front(T* v) {
            m_head.insert_after(to_node(v));
        }

        void erase(T* v) {
            to_node(v)->erase();
        }

        Iterator begin() const {
            return Iterator(m_head.m_next);
        }
        Iterator end() const {
            return Iterator(&m_head);
        }

        static T* from_node(Node<M> *m) { 
            return static_cast<T*>(m);
        }

        static Node<M>* to_node(T *m) { 
            return static_cast<Node<M>*>(m);
        }

        class Iterator {
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = T*;
            using pointer = T*;
            using reference = T&;

            reference operator*() const { return *from_node(m_pos); }
            pointer operator->() { return from_node(m_pos); }

            Iterator& operator++() { m_pos = m_pos->m_next; return *this; }  
            Iterator& operator--() { m_pos = m_pos->m_prev; return *this; }  

            friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_pos == b.m_pos; };
            friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_pos != b.m_pos; };     
        private:
            Iterator(GenericNode *pos): m_pos(pos) {}
            GenericNode *m_pos;
        };
    private:
        GenericNode m_head;
    };
}