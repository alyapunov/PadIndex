#pragma once

#include <assert.h>

#include <climits>
#include <functional>
#include <vector>

namespace target {

// dynamic_bitset.
//
// The class represents a dynamic array of bits.
// Generally it follows boost::dynamic_bitset API (see boost man for best description)
// but with some extensions (see Extension section, I hope it's commented well enough).
// Also some paranoiac checks were added, like out-of-range access.
//
// Implementation of own dynamic_bitset with the availability of boost::dynamic_bitset
//  was caused by several reasons:
// 1. boost::dynamic_bitset have unoptimized bit functions, such as popcount
//  (dynamic_bitset::count) and bit search (find_first/find_next).
//  See next commit for a benchmark.
// 2. boost::dynamic_bitset allows no access to word array, making it hardly possible
//  for hash calculation of the entire bitset.
// 3. There are some other possible optimization, e.g. all-zero and all-ones special
//  bitset cases that couldn't even be proved as useful because of private and complicated
//  implementation of boost::bitset.
//
// Some terminology:
// The bitset stores bits in vector of words (unsigned long long now).
// If bitset size is multiple of number of bits in word then the vector holds
//  exactly (bitset_size / sizeof(word)) words with all word bits bit used by the bitset.
//  Let's name both bitset and every its word as 'complete'.
// Otherwise, the vector consists of (bitset_size / sizeof(word) + 1) words with the
//  last word not fully used. Let's name such a bitset and its last word as 'incomplete',
//  while first (bitset_size / sizeof(word)) words will be 'complete'.
// Note that in any case a bitset has exactly (bitset_size / sizeof(word)) complete words.
// Let's call unused bits of incomplete word as 'excess', all other bits - 'payload' bits.
// Excess bits are usually in undefined state.

class dynamic_bitset
{
public:
    using size_type = size_t;
    static constexpr size_type npos = SIZE_MAX;

    // Initialization.
    dynamic_bitset() = default;

    explicit dynamic_bitset(size_t aSize, bool aBit = false)
    {
        resize(aSize, aBit);
    }

    void resize(size_t aSize, bool aBit = false)
    {
        if (0 != m_PayloadMask && aSize > m_Size)
        {
            // Set excess bits that was allocated before and are in undefined state now.
            if (aBit)
                m_Bits[m_CompleteCount] |= ~m_PayloadMask;
            else
                m_Bits[m_CompleteCount] &= m_PayloadMask;
        }
        size_t sNewVectorSize = (aSize + WORD_BITS - 1) / WORD_BITS;
        m_Bits.resize(sNewVectorSize, aBit ? WORD_MAX : 0);
        m_Size = aSize;
        m_CompleteCount = m_Size / WORD_BITS;
        size_t sPayloadBits = m_Size % WORD_BITS;
        word_t sExcessMask = WORD_MAX << sPayloadBits;
        m_PayloadMask = ~sExcessMask;
    }

    // Overall getters and setters
    size_t size() const
    {
        return m_Size;
    }

    bool empty() const
    {
        return 0 == m_Size;
    }

    bool all() const
    {
        for (size_t i = 0; i < m_CompleteCount; i++)
            if (0 != ~m_Bits[i])
                return false;
        if (0 != m_PayloadMask)
            if (0 != (m_PayloadMask & ~m_Bits[m_CompleteCount]))
                return false;
        return true;
    }

    bool any() const
    {
        for (size_t i = 0; i < m_CompleteCount; i++)
            if (0 != m_Bits[i])
                return true;
        if (0 != m_PayloadMask)
            if (0 != (m_PayloadMask & m_Bits[m_CompleteCount]))
                return true;
        return false;
    }

    bool none() const
    {
        return !any();
    }

    size_t count() const
    {
        size_t sRes = 0;
        for (size_t i = 0; i < m_CompleteCount; i++)
            sRes += bitscount(m_Bits[i]);
        if (0 != m_PayloadMask)
            sRes += bitscount(m_PayloadMask & m_Bits[m_CompleteCount]);
        return sRes;
    }

    bool operator==(const dynamic_bitset& aBitset) const
    {
        assert(m_Size == aBitset.m_Size);
        for (size_t i = 0; i < m_CompleteCount; i++)
            if (m_Bits[i] != aBitset.m_Bits[i])
                return false;
        if (0 != m_PayloadMask)
            if (0 != (m_PayloadMask & (m_Bits[m_CompleteCount] ^ aBitset.m_Bits[m_CompleteCount])))
                return false;
        return true;
    }

    void set()
    {
        std::fill(m_Bits.begin(), m_Bits.end(), word_t(WORD_MAX));
    }

    void reset()
    {
        std::fill(m_Bits.begin(), m_Bits.end(), 0);
    }

    void clear()
    {
        resize(0);
    }

    void flip()
    {
        for (word_t& sWord : m_Bits)
            sWord = ~sWord;
    }

    // Bit getters and setters.
    bool test(size_t aPos) const
    {
        assert(aPos < m_Size);
        return 0 != (word(aPos) & bit(aPos));
    }

    bool operator[](size_t aPos) const
    {
        return test(aPos);
    }

    class BitHandler
    {
    public:
        BitHandler(dynamic_bitset& aBitset, size_t aPos) : m_Bitset(aBitset), m_Pos(aPos) {}
        BitHandler& operator=(bool aValue) { m_Bitset.set(m_Pos, aValue); return *this; }
        BitHandler& operator=(const BitHandler& aValue) { *this = (bool)aValue; return *this; }
        bool operator==(const BitHandler& a) const { return m_Bitset.test(m_Pos) == a.m_Bitset.test(a.m_Pos); }
        operator bool() const { return m_Bitset.test(m_Pos); }
    private:
        dynamic_bitset& m_Bitset;
        size_t m_Pos;
    };

    BitHandler operator[](size_t aPos)
    {
        assert(aPos < m_Size);
        return BitHandler(*this, aPos);
    }

    void set(size_t aPos)
    {
        assert(aPos < m_Size);
        word(aPos) |= bit(aPos);
    }

    void set(size_t aPos, bool aBit)
    {
        assert(aPos < m_Size);
        aBit ? set(aPos) : reset(aPos);
    }

    void reset(size_t aPos)
    {
        assert(aPos < m_Size);
        word(aPos) &= ~bit(aPos);
    }

    void flip(size_t aPos)
    {
        assert(aPos < m_Size);
        word(aPos) ^= bit(aPos);
    }

    // Iteration.
    size_t find_first() const
    {
        return find_first(0);
    }

    size_t find_next(size_t aPos) const
    {
        word_t sMask = bit(aPos);
        sMask |= sMask - 1;
        size_t sWordNo = aPos / WORD_BITS;
        word_t sWord = m_Bits[sWordNo] & ~sMask;
        return 0 != sWord ? get_pos(sWordNo, sWord) : find_first(sWordNo + 1);
    }

    // Inplace operations.
    dynamic_bitset& operator&=(const dynamic_bitset& aBitset)
    {
        assert(m_Size == aBitset.m_Size);
        for (size_t i = 0; i < m_Bits.size(); i++)
            m_Bits[i] &= aBitset.m_Bits[i];
        return *this;
    }

    dynamic_bitset& operator|=(const dynamic_bitset& aBitset)
    {
        assert(m_Size == aBitset.m_Size);
        for (size_t i = 0; i < m_Bits.size(); i++)
            m_Bits[i] |= aBitset.m_Bits[i];
        return *this;
    }

    dynamic_bitset& operator-=(const dynamic_bitset& aBitset)
    {
        assert(m_Size == aBitset.m_Size);
        for (size_t i = 0; i < m_Bits.size(); i++)
            m_Bits[i] &= ~aBitset.m_Bits[i];
        return *this;
    }

    // Result operations.
    friend dynamic_bitset operator&(const dynamic_bitset& aBitset1, const dynamic_bitset& aBitset2)
    {
        dynamic_bitset sRes(aBitset1);
        sRes &= aBitset2;
        return sRes;
    }

    friend dynamic_bitset operator|(const dynamic_bitset& aBitset1, const dynamic_bitset& aBitset2)
    {
        dynamic_bitset sRes(aBitset1);
        sRes |= aBitset2;
        return sRes;
    }

    friend dynamic_bitset operator-(const dynamic_bitset& aBitset1, const dynamic_bitset& aBitset2)
    {
        dynamic_bitset sRes(aBitset1);
        sRes -= aBitset2;
        return sRes;
    }

    dynamic_bitset operator~() const
    {
        dynamic_bitset sRes(*this);
        sRes.flip();
        return sRes;
    }

    // Extensions.
    // Size of dynamically allocated memory.
    size_t mem_size() const
    {
        return sizeof(word_t) * m_Bits.capacity();
    }

    // Hash of the bitset.
    size_t hash() const
    {
        size_t sRes = 0;
        for (size_t i = 0; i < m_CompleteCount; i++)
            sRes ^= m_Bits[i];
        if (0 != m_PayloadMask)
            sRes ^= m_PayloadMask & m_Bits[m_CompleteCount];
        return sRes;
    }

private:
    using word_t = unsigned long long;
    static constexpr size_t WORD_BITS = sizeof(word_t) * CHAR_BIT;
    static constexpr size_t WORD_MAX = static_cast<word_t>(-1);

    // A couple of methods for simplification of bit access.
    word_t& word(size_t aPos)
    {
        return m_Bits[aPos / WORD_BITS];
    }

    const word_t& word(size_t aPos) const
    {
        return m_Bits[aPos / WORD_BITS];
    }

    static word_t bit(size_t aPos)
    {
        return word_t(1) << (aPos % WORD_BITS);
    }

    // Finds lowest bit position bit in a word and caclulate its position in bitset.
    // Returns npos if out-of-bound.
    size_t get_pos(size_t aWordNo, word_t aWord) const
    {
        size_t sRes = aWordNo * WORD_BITS + ctz(aWord);
        return sRes < m_Size ? sRes : npos;
    }

    // Finds first bit position, starting with word position.
    size_t find_first(size_t aStartWordNo) const
    {
        for (size_t i = aStartWordNo; i < m_Bits.size(); i++)
            if (0 != m_Bits[i])
                return get_pos(i, m_Bits[i]);
        return npos;
    }

    // Bit functions.
    // Population count.
    static size_t bitscount(word_t aWord)
    {
        return __builtin_popcountll(aWord);
    }

    // Count trailing zeros.
    static size_t ctz(word_t aWord)
    {
        return __builtin_ctzll(aWord);
    }

    // Bits packed in words.
    std::vector<word_t> m_Bits;
    // Number of bits in bitset.
    size_t m_Size = 0;
    // Count of complete words.
    size_t m_CompleteCount = 0;
    // Mask that have ones in places of payload bits in incomplete word.
    // Is zero if the bitset is complete.
    word_t m_PayloadMask = 0;
};

} // namespace target {

namespace std
{
    template<>
    struct hash<target::dynamic_bitset>
    {
        size_t operator()(const target::dynamic_bitset& a) const
        {
            return a.hash();
        }
    };
} // namespace std
