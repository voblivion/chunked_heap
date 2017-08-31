/*
A chuked_heap is a mix between a heap, a vector and a list :
- Like an std::vector a chunked_heap is cache friendly (or at least it tends to be if its size doesn't shrink too much
- Like an std::list the element inside a chunked_heap will never move until erased : this allow pointers to them
- Unlike an std::vector elements are not all in a contiguous memory location
- Unlike an std::list or an std::vector you never know where will be the new emplaced element in a chunked_heap : it
might be at the end but nothing is less certain

Use cases :
- the container needs to be iterated through, as fast as possible
- the elements must be reference-able from somewhere else and, therefor, never move

In the cheat sheet of containers :
- Order is important : no
- Need to find element by key : no
- Need to merge collections : possibly (not available now)
- (additional) Need to be iterated through quickly : yes

NOTE : in addition to the element type, a ChunkSizer should be given to the template. A ChunkSizer is a callable object that
determines, for a number of already allocated chunks given, the size of the next chunk. For now, this call must be stateless.
*/
#pragma once
#include <list>

namespace vob {
    template <size_t N>
    struct constant_chunk_sizer {
        size_t operator()(size_t chunks_allocated_count) const {
            return N;
        }
    };

    template <size_t M, size_t N>
    struct planned_chunk_sizer {
        size_t operator()(size_t chunks_allocated_count) const {
            return chunks_allocated_count == 0 ? M : N;
        }
    };

    template <typename T, typename ChunkSizer = constant_chunk_sizer<8>, typename Allocator = std::allocator<T>>
    class chunked_heap {
        // CLASSES
        struct cool_chunk {
            cool_chunk(Allocator& object_allocator, std::allocator<size_t>& index_allocator, std::allocator<cool_chunk*> chunk_ptr_allocator, size_t capacity)
                : m_object_allocator(object_allocator)
                , m_index_allocator(index_allocator)
                , m_chunk_ptr_allocator(chunk_ptr_allocator)
                , m_capacity(capacity) {
                m_data = m_object_allocator.allocate(m_capacity);
                m_next_index = m_index_allocator.allocate(m_capacity);
                m_prev_index = m_index_allocator.allocate(m_capacity);
                m_next_chunk = m_chunk_ptr_allocator.allocate(m_capacity);
                m_prev_chunk = m_chunk_ptr_allocator.allocate(m_capacity);
            }
            ~cool_chunk() {
                m_object_allocator.deallocate(m_data, m_capacity);
                m_index_allocator.deallocate(m_next_index, m_capacity);
                m_index_allocator.deallocate(m_prev_index, m_capacity);
                m_chunk_ptr_allocator.deallocate(m_next_chunk, m_capacity);
                m_chunk_ptr_allocator.deallocate(m_prev_chunk, m_capacity);

            }
            size_t m_capacity;
            T* m_data;
            Allocator& m_object_allocator;
            std::allocator<size_t>& m_index_allocator;
            std::allocator<cool_chunk*>& m_chunk_ptr_allocator;
            size_t* m_next_index;
            size_t* m_prev_index;
            cool_chunk** m_next_chunk;
            cool_chunk** m_prev_chunk;
        };

    public:
        // CLASSES
        class iterator {
            friend class chunked_heap<T, ChunkSizer, Allocator>;
        public:
            typedef std::forward_iterator_tag iterator_category;

            // CONSTRUCTORS
            iterator()
                : m_chunk(nullptr) {
            }

            // OPERATORS
            iterator& operator++() {
                auto index = m_index;
                m_index = m_chunk->m_next_index[index];
                m_chunk = m_chunk->m_next_chunk[index];
                return *this;
            }
            iterator& operator=(const iterator& other) {
                m_chunk = other.m_chunk;
                m_index = other.m_index;
                return *this;
            }
            bool operator==(const iterator& other) const {
                return m_chunk == other.m_chunk && m_index == other.m_index;
            }
            bool operator!=(const iterator& other) const {
                return m_chunk != other.m_chunk || m_index != other.m_index;
            }
            T& operator*() const {
                return (m_chunk->m_data[m_index]);
            }
            T* operator->() const {
                return &(m_chunk->m_data[m_index]);
            }

        private:
            // CONSTRUCTORS
            iterator(cool_chunk* chunk, size_t i)
                : m_chunk(chunk)
                , m_index(i) {
            }

            // ATTRIBUTES
            cool_chunk* m_chunk;
            size_t m_index;
        };
        class const_iterator {
            friend class chunked_heap<T, ChunkSizer, Allocator>;
        public:
            typedef std::forward_iterator_tag iterator_category;

            // CONSTRUCTORS
            const_iterator()
                : m_chunk(nullptr) {
            }

            // OPERATORS
            const_iterator& operator++() {
                auto index = m_index;
                m_index = m_chunk->m_next_index[index];
                m_chunk = m_chunk->m_next_chunk[index];
                return *this;
            }
            const_iterator& operator=(const const_iterator& other) {
                m_chunk = other.m_chunk;
                m_index = other.m_index;
                return *this;
            }
            bool operator==(const const_iterator& other) const {
                return m_chunk == other.m_chunk && m_index == other.m_index;
            }
            bool operator!=(const const_iterator& other) const {
                return m_chunk != other.m_chunk || m_index != other.m_index;
            }
            const T& operator*() const {
                return (m_chunk->m_data[m_index]);
            }
            const T* operator->() const {
                return &(m_chunk->m_data[m_index]);
            }

        private:
            // CONSTRUCTORS
            const_iterator(cool_chunk* c, size_t i)
                : m_chunk(c)
                , m_index(i) {
            }

            // ATTRIBUTES
            cool_chunk* m_chunk;
            unsigned m_index;
        };

        // CONSTRUCTORS
        chunked_heap()
            : m_size(0)
            , m_last_inserted_index(0)
            , mp_first_inserted_chunk(nullptr)
            , m_first_inserted_index(0) {
        }

        // DESTRUCTOR
        ~chunked_heap() {
            for (auto it = begin(); it != end(); ++it) {
                erase(it);
            }
        }

        // METHODS
        template <typename... Args>
        iterator emplace(Args... args) {
            auto it_it_free_space = m_free_spaces.rbegin();
            iterator it_free_space;
            if (it_it_free_space == m_free_spaces.rend()) {
                cool_chunk* p_prev_chunk = nullptr;
                if (!m_chunks.empty()) {
                    p_prev_chunk = &m_chunks.back();
                }
                size_t index = m_last_inserted_index + 1;
                if (!mp_first_inserted_chunk || index == m_chunk_sizer(m_chunks.size() - 1)) {
                    // No free space at end of last chunk, create new chunk
                    m_chunks.emplace_back(m_allocator, m_index_allocator, m_chunk_ptr_allocator, m_chunk_sizer(m_chunks.size()));
                    index = 0;
                }
                auto& chunked_heap = m_chunks.back();
                it_free_space = iterator(&chunked_heap, index);
                chunked_heap.m_prev_chunk[index] = p_prev_chunk;
                chunked_heap.m_prev_index[index] = m_last_inserted_index;
                chunked_heap.m_next_chunk[index] = nullptr;
                chunked_heap.m_next_index[index] = 0;

                m_last_inserted_index = index;
            }
            else {
                it_free_space = *it_it_free_space;
                m_free_spaces.pop_back();
            }
            return emplace(it_free_space, std::forward<Args>(args)...);
        }
        void erase(iterator it) {
            cool_chunk* p_prev_chunk = it.m_chunk->m_prev_chunk[it.m_index];
            cool_chunk* p_next_chunk = it.m_chunk->m_next_chunk[it.m_index];
            auto prev_index = it.m_chunk->m_prev_index[it.m_index];
            auto next_index = it.m_chunk->m_next_index[it.m_index];

            if (p_prev_chunk) {
                p_prev_chunk->m_next_index[prev_index] = next_index;
                p_prev_chunk->m_next_chunk[prev_index] = p_next_chunk;
            }
            if (p_next_chunk) {
                p_next_chunk->m_prev_index[next_index] = prev_index;
                p_next_chunk->m_prev_chunk[next_index] = p_prev_chunk;
            }

            // update begin iterator info if needed
            if (it.m_chunk == mp_first_inserted_chunk && it.m_index == m_first_inserted_index) {
                mp_first_inserted_chunk = p_next_chunk;
                m_first_inserted_index = next_index;
            }
            it.m_chunk->m_data[it.m_index].~T();
            --m_size;
            m_free_spaces.emplace_back(it);
        }
        iterator begin() {
            if (m_chunks.size() == 0) {
                return iterator(nullptr, 0);
            }
            return iterator(&m_chunks.front(), m_first_inserted_index);
        }
        const_iterator begin() const {
            if (m_chunks.size() == 0) {
                return const_iterator(nullptr, 0);
            }
            return const_iterator(&m_chunks.front(), m_first_inserted_index);
        }
        iterator end() {
            return iterator(nullptr, 0);
        }
        const_iterator end() const {
            return const_iterator(nullptr, 0);
        }

    private:
        // METHODS
        template <typename... Args>
        iterator emplace(iterator it, Args... args) {
            cool_chunk* p_prev_chunk = it.m_chunk->m_prev_chunk[it.m_index];
            cool_chunk* p_next_chunk = it.m_chunk->m_next_chunk[it.m_index];
            auto prev_index = it.m_chunk->m_prev_index[it.m_index];
            auto next_index = it.m_chunk->m_next_index[it.m_index];

            if (p_prev_chunk) {
                p_prev_chunk->m_next_index[prev_index] = it.m_index;
                p_prev_chunk->m_next_chunk[prev_index] = it.m_chunk;
            }
            if (p_next_chunk) {
                p_next_chunk->m_prev_index[next_index] = it.m_index;
                p_next_chunk->m_prev_chunk[next_index] = it.m_chunk;
            }

            // update begin iterator info if needed
            if (p_next_chunk == mp_first_inserted_chunk && next_index == m_first_inserted_index) {
                mp_first_inserted_chunk = it.m_chunk;
                m_first_inserted_index = it.m_index;
            }
            new(&it.m_chunk->m_data[it.m_index]) T(std::forward<Args>(args)...);
            ++m_size;
            return it;
        }

        // ATTRIBUTES
        size_t m_size;
        std::list<cool_chunk> m_chunks;
        std::list<iterator> m_free_spaces;
        Allocator m_allocator;
        std::allocator<size_t> m_index_allocator;
        std::allocator<cool_chunk*> m_chunk_ptr_allocator;
        ChunkSizer m_chunk_sizer;

        // to remember last used index in last allocated chunk
        size_t m_last_inserted_index;

        // to create begin iterator
        size_t m_first_inserted_index;
        cool_chunk* mp_first_inserted_chunk;
    };
}
