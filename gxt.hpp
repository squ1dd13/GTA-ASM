//
// Created by Alex Gallon on 16/07/2020.
//

#ifndef GTASM_GXT_HPP
#define GTASM_GXT_HPP

// I'm lazy, OK?
#define init_read(varname) stream.read((char *)&gxt.varname, sizeof(gxt.varname))

#define POLY 0x82f63b78

/* CRC-32 (Ethernet, ZIP, etc.) polynomial in reversed bit order. */
/* #define POLY 0xedb88320 */

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

struct GXT {
    uint16_t version;
    uint16_t encoding; // 8/16 for ASCII/UTF-8

    struct KeyTable {
        char tkeyStr[4];
        uint32_t blockSize;

        struct KeyTableEntry {
            uint32_t entryOffset;
            uint32_t nameCRC32;
        };

        // blockSize / 8
        KeyTableEntry *entries {};

        virtual ~KeyTable() {
            delete[] entries;
        }
    };

    struct TableBlock {
        char tableStr[4];
        uint32_t blockSize;

        struct SubtableEntry {
            char name[8];
            uint32_t offset;

            KeyTable keyTable;
        };

        // blockSize / 12
        SubtableEntry *entries {};

        virtual ~TableBlock() {
            delete[] entries;
        }
    };

    TableBlock tableBlock;

    static GXT read(string_ref filename) {
        const char *searchStr = "FEP_OPT";
        //std::crc

        uint32_t crc = 3810842265;//crc32c(0, (const unsigned char *)searchStr, 8);

        GXT gxt;

        std::ifstream stream(filename, std::ios::binary);
        init_read(version);
        init_read(encoding);

        stream.read(gxt.tableBlock.tableStr, 4);
        init_read(tableBlock.blockSize);

        auto numSubtables = gxt.tableBlock.blockSize / 12;
        gxt.tableBlock.entries = new TableBlock::SubtableEntry[numSubtables];

        for(int i = 0; i < numSubtables; ++i) {
            auto &entry = gxt.tableBlock.entries[i];

            stream.read(entry.name, 8);
            init_read(tableBlock.entries[i].offset);

            // Move to the key table and read that, then go back.
            auto savedPos = stream.tellg(); {
                stream.seekg(entry.offset);

                if(std::strcmp(entry.name, "MAIN") != 0) {
                    // Not the main table, so 'read' the name (skip it).
                    stream.ignore(8);
                }

                stream.read(gxt.tableBlock.entries[i].keyTable.tkeyStr, 4);
                init_read(tableBlock.entries[i].keyTable.blockSize);

                auto &keyTable = gxt.tableBlock.entries[i].keyTable;

                auto keyEntries = keyTable.blockSize / 8;
                keyTable.entries = new KeyTable::KeyTableEntry[keyEntries];

                for(int j = 0; j < keyEntries; ++j) {
                    init_read(tableBlock.entries[i].keyTable.entries[j].entryOffset);
                    init_read(tableBlock.entries[i].keyTable.entries[j].nameCRC32);

                    if(keyTable.entries[j].nameCRC32 == crc) {
                        std::cout << "yes\n";
                        exit(0);
                    }

                    std::cout << keyTable.entries[j].nameCRC32 << '\n';
                }
            } stream.seekg(savedPos);

            std::cout << entry.name << '\n';
        }

        return gxt;
    }
};

#undef init_read

#endif //GTASM_GXT_HPP
