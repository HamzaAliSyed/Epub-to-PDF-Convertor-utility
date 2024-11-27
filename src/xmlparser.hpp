#pragma once

#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <cstdint> 
#include <stdexcept>
#include <memory>
#include <cstddef>

inline auto XMLCheck(bool condition, const std::u8string_view message) -> void {
    if (!condition) {
        throw std::runtime_error(std::string(reinterpret_cast<const char*>(message.data())));
    }
}

inline constexpr std::size_t MAX_ELEMENT_DEPTH = 500;

class StringPair {
    public:
        enum class ProcessingMode : std::uint8_t {
            EntityProcessing = 0x01,
            NewlineNormalization = 0x02,
            WhiteSpaceCollapsing = 0x04,
            ValidateUTF8 = 0x08,

            TextElement = static_cast<std::uint8_t>(EntityProcessing) | static_cast<std::uint8_t>(NewlineNormalization) | static_cast<std::uint8_t>(ValidateUTF8),
            TextElementLeaveEntities = static_cast<std::uint8_t>(NewlineNormalization),
            AttributeName = 0,
            AttributeValue = TextElement,
            AttributeValueLeaveEntities = static_cast<std::uint8_t>(NewlineNormalization),
            Comment = static_cast<std::uint8_t>(NewlineNormalization)
        };

        StringPair() : _flags(0),_text() {}
        ~StringPair() = default;

        auto Set(std::u8string_view text, std::uint8_t flags) -> void {
            Reset();
            _text = std::u8string(text);
            _flags = static_cast<std::uint16_t>(flags) | static_cast<std::uint16_t>(InternalFlags::NeedsFlush);
        }

        auto GetString() const -> std::u8string_view {
            return _text;
        }

        [[nodiscard]] auto Empty() const -> bool {
            return _text.empty();
        }

        auto SetInternalString(std::u8string_view inputString) -> void {
            Reset();
            _text = std::u8string(inputString);
        }

        auto SetString(std::u8string_view inputString, ProcessingMode flags = ProcessingMode{}) -> void {
            Reset();
            _text = std::u8string(inputString);
            _flags = static_cast<std::uint16_t>(flags) | static_cast<std::uint16_t>(InternalFlags::NeedsFlush);
        }

        auto ParseText(std::u8string_view input,std::u8string_view endTag,ProcessingMode flags, int& lineNumber) -> std::u8string {
            if (input.empty() || endTag.empty()) {
                throw std::invalid_argument("Input or endTag is empty");
            }

            std::u8string result;
            const char8_t endCharacter = endTag[0];
            size_t position = 0;

            while(position < input.length()) {
                if (input[position] == endCharacter && position + endTag.length() <= input.length() && input.substr(position, endTag.length()) == endTag) {
                    _text = result;
                    _flags = static_cast<uint16_t>(flags);
                    return std::u8string(input.substr(position + endTag.length()));
                } else if (input[position] == u8'\n') {
                    ++lineNumber;
                }
                result += input[position];
                ++position;
            }
            return std::u8string();
        }
        
        auto ParseName(std::u8string_view input) -> std::u8string {
            if (input.empty()) {
                return std::u8string();
            }

            auto isNameStart = [](char8_t character) {
                return std::isalpha(static_cast<unsigned char>(character)) || character == u8'_' || character == u8':';
            };

            auto isNameCharacter = [](char8_t character) {
                return std::isalnum(static_cast<unsigned char>(character)) || character == u8'_' || character == u8':' || character == u8'-' || character == u8'.';
            };

            if(!isNameStart(input[0])) {
                return std::u8string();
            }

            size_t position = 1;
            while (position < input.length() && isNameCharacter(input[position])) {
                ++position;
            }

            _text = std::u8string(input.substr(0, position));
            _flags = 0;

            return std::u8string(input.substr(position));
        }

        auto TransferTo(StringPair& other) -> void {
            if (this == &other) {
                return;
            }

            other.Reset();

            other._flags = _flags;
            other._text = std::move(_text);

            _flags = 0;
            _text.clear();
        }
    
    private:
        std::uint16_t _flags;
        std::u8string _text;

        enum class InternalFlags : std::uint16_t {
            NeedsFlush = 0x100,
            NeedsDelete = 0x200
        };

        StringPair(const StringPair&) = delete;
        StringPair& operator=(const StringPair&) = delete;

        void Reset() {
            _text.clear();
            _flags = 0;
        }

        void CollapseWhitespace() {
            if ((_flags & static_cast<uint16_t>(InternalFlags::NeedsDelete)) != 0) {
                return;
            }

            auto isWhiteSpace = [](char8_t character) {
                return character == u8' ' || character == u8'\t' || character == u8'\n' || character == u8'\r';
            };

            size_t start = 0;
            while (start < _text.length() && isWhiteSpace(_text[start])) {
                ++start;
            }

            _text = _text.substr(start);
            if (_text.empty()) {
                return;
            }

            std::u8string result;
            bool wasSpace = false;

            for (char8_t eachCharacter : _text) {
                if (isWhiteSpace(eachCharacter)) {
                    if (!wasSpace) {
                        result += u8' ';
                        wasSpace = true;
                    }
                } else {
                    result += eachCharacter;
                    wasSpace = false;
                }
            }

            if (!result.empty() && result.back() == u8' '){
                result.pop_back();
            }

            _text = std::move(result);
        }
};

template <typename T, std::size_t INITIAL_SIZE>
class CustomResizingArray {
    public:
        CustomResizingArray() : _memory(_pool), _allocated(INITIAL_SIZE), _size(0) {}

        ~CustomResizingArray() {
            if (_memory != _pool) {
                delete[] _memory;
            }
        }

        auto Clear() noexcept -> void {
            _size = 0;
        }

        auto Push(const T& value) -> void {
            if (_size >= std::numeric_limits<int>::max()) {
                throw std::length_error("Array size would exceed maximum");
            }
            EnsureCapacity(_size + 1);
            _memory[_size] = value;
            ++_size;
        }

        auto PushArray(std::size_t count) -> T* {
            if(_size > (std::numeric_limits<size_t>::max() - count)) {
                throw std::length_error("Array size would exceed maximum");
            }
            EnsureCapacity(_size + count);
            T* result = &_memory[_size];
            _size += count;
            return result;
        }

        auto Pop() -> T {
            if (_size == 0) {
                throw std::out_of_range("Cannot pop from empty array");
            }
            --_size;
            return _memory[_size];
        }

        auto PopArray(std::size_t count) -> void {
            if (_size < count) {
                throw std::out_of_range("Cannot pop more elements than exist");
            }
            _size -= count;
        }

        [[nodiscard]] auto Empty() const noexcept -> bool {
            return _size == 0;
        }

        auto operator[](std::size_t index) -> T& {
            if (index >= _size) {
                throw std::out_of_range("Index out of bounds");
            }
            return _memory[index];
        }

        auto operator[](std::size_t index) const -> const T& {
            if (index >= _size) {
                throw std::out_of_range("Index out of bounds");
            }
            return _memory[index];
        }

        [[nodiscard]] auto PeekTop() const -> const T& {
            if (_size == 0) {
                throw std::out_of_range("Cannot peek an empty array");
            }
            return _memory[_size-1];
        }

        [[nodiscard]] auto Size() const noexcept -> std::size_t {
            return _size;
        }

        [[nodiscard]] auto Capacity() const noexcept -> std::size_t {
            return _allocated;
        }

        auto SwapRemove(std::size_t index) -> void {
            if (_size == 0) {
                throw std::out_of_range("Cannot remove from empty array");
            }

            if (index >= _size) {
                throw std::out_of_range("Index out of bounds");
            }

            _memory[index] = _memory[_size -1];
            --_size;
        }

        [[nodiscard]] auto Data() const noexcept -> const T* {
            return _memory;
        }

        [[nodiscard]] auto Data() noexcept -> T* {
            return _memory;
        }

        auto operator<=>(const CustomResizingArray&) const = delete;
        auto operator==(const CustomResizingArray&) const = delete;

    private:
        CustomResizingArray(const CustomResizingArray&) = delete;
        CustomResizingArray& operator=(const CustomResizingArray&) = delete;

        T* _memory;
        T  _pool[INITIAL_SIZE];
        std::size_t _allocated;
        std::size_t _size;

        auto EnsureCapacity(std::size_t requiredCapacity) -> void {
            if (requiredCapacity == 0) {
                throw std::invalid_argument("Capacity must be greater than 0");
            }

            if (requiredCapacity > _allocated) {
                if (requiredCapacity > SIZE_MAX / 2 / sizeof(T)) {
                    throw std::length_error("Requested capacity too large");
                }

                const std::size_t newCapacity = requiredCapacity * 2;
                auto newMemory = std::make_unique<T[]>(newCapacity);

                if (newCapacity < _size) {
                    throw std::runtime_error("New capacity smaller than current size");
                }

                std::copy(_memory, _memory + _size, newMemory.get());

                if (_memory != _pool) {
                    delete[] _memory;
                }

                _memory = newMemory.release();
                _allocated = newCapacity;
            }
        }
};

class MemoryPool {
    public:
        MemoryPool() = default;
        virtual ~MemoryPool() = default;

        virtual size_t GetItemSize() const = 0;
        virtual void* Allocate() = 0;
        virtual void  Free(void*) = 0;
        virtual void  SetTracked() = 0;
};

template <size_t ItemSize>
class MemoryPoolTemplate : public MemoryPool {
    public:
        MemoryPoolTemplate() = default;

        ~MemoryPoolTemplate() {
            Clear();
        }

        auto Clear() -> void {
            while (!blockPointers.Empty()) {
                delete blockPointers.Pop();
            }

            root = nullptr;
            currentAllocations = 0;
            totalAllocations = 0;
            maxAllocations = 0;
            untrackedAllocations = 0;
        }

        auto GetItemSize() const -> size_t override {
            return ItemSize;
        }

        auto GetCurrentAllocations() const -> size_t {
            return currentAllocations;
        }

        auto Allocate() -> void* override {
            if (!root) {
                auto *block = new Block;
                blockPointers.Push(block);

                auto *blockItems = block->items.data();
                for (size_t index =0; index < ITEMS_PER_BLOCK - 1; ++index) {
                    blockItems[index].next = &(blockItems[index + 1]);
                }
                blockItems[ITEMS_PER_BLOCK - 1].next = nullptr;
                root = blockItems;
            }
            auto* result = root;
            root = root->next;

            ++currentAllocations;
            maxAllocations = std::max(currentAllocations, maxAllocations);
            ++totalAllocations;
            ++untrackedAllocations;

            return result;
        }

        auto Free(void* memory) -> void override {
            if (!memory) {
                return ;
            }

            --currentAllocations;
            auto item = static_cast<Item*>(memory);

            item -> next = root;
            root = item;
        }

        auto Trace(std::string_view name) const -> void {
            std::printf("Memory Pool %s:\n"
                "Watermark: %zu (%zuKB)\n"
                "Current: %zu\n"
                "Item Size: %zu\n"
                "Total Allocations: %zu\n"
                "Blocks: %zu\n",
                name.data(),
                maxAllocations, maxAllocations * ItemSize / 1024,
                currentAllocations,
                ItemSize,
                totalAllocations,
                blockPointers.Size());
        }

        auto SetTracked() -> void override {
            --untrackedAllocations;
        }

        auto GetUntrackedAllocations() const -> size_t {
            return untrackedAllocations;
        }

    private:
        MemoryPoolTemplate(const MemoryPoolTemplate&) = delete;
        MemoryPoolTemplate& operator=(const MemoryPoolTemplate&) = delete;

        static constexpr size_t ITEMS_PER_BLOCK = (4 * 1024) / ItemSize;

        union Item {
            Item* next;
            std::array<std::byte, ItemSize> data;
        };

        struct Block {
            std::array<Item, ITEMS_PER_BLOCK> items;
        };

        CustomResizingArray<Block*> blockPointers;
        Item* root{nullptr};

        size_t currentAllocations{0};
        size_t totalAllocations{0};
        size_t maxAllocations{0};
        size_t untrackedAllocations{0};
};

class XMLUtilities {
    public:
    private:
        static constexpr std::u8string_view writeBoolTrue{u8"true"};
        static constexpr std::u8string_view writeBoolFalse{u8"false"};

        XMLUtilities() = delete;
        XMLUtilities(const XMLUtilities&) = delete;
        XMLUtilities& operator=(const XMLUtilities&) = delete;
        ~XMLUtilities() = delete;
};