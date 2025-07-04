#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <bitset>
#include <cstring>
#include <algorithm>

using namespace std;

struct RGBValue {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool isNull = true;

    RGBValue(uint8_t r, uint8_t g, uint8_t b) {
        red = r;
        green = g;
        blue = b;
        isNull = false;
    }

    RGBValue() {
        isNull = true;
    }

    bool operator==(const RGBValue& other) const {
        return (red == other.red && green == other.green) && (blue == other.blue);
    }

    void print() {
        cout << (int)red << ' ' << (int)green << ' ' << (int)blue << endl;
    }

    uint8_t hash() const {
        return (red*3 + green*5 + blue*7) % 64;
    }
};

class QOI_Image {
private:
    RGBValue index[64];
    vector<RGBValue> m_RGBBytes;
    vector<uint8_t> m_QOIBytes;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_channels;
    uint32_t m_colorspace;

public:
    QOI_Image() {
        for (int i=0; i<64; i++)
            index[i] = RGBValue(); // initialize to NULL
    }
    
    void readBMP(const string& filename, int channels=3, int colorspace=0) {
        m_RGBBytes = {};
        m_channels = channels;
        m_colorspace = colorspace;

        ifstream file(filename, ios::binary);
        if (!file) {
            cerr << "Failed to open BMP file." << endl;
            return;
        }

        file.seekg(10);
        uint32_t dataOffset;
        file.read(reinterpret_cast<char*>(&dataOffset), 4);

        file.seekg(18);
        file.read(reinterpret_cast<char*>(&m_width), 4);
        file.read(reinterpret_cast<char*>(&m_height), 4);

        file.seekg(dataOffset);

        int rowPadded = (m_width * 3 + 3) & (~3);
        vector<uint8_t> row(rowPadded);
        m_RGBBytes.reserve(m_width * m_height * 3 + 9);

        for (int y = 0; y < m_height; y++) {
            file.read(reinterpret_cast<char*>(row.data()), rowPadded);
            for (int x = 0; x < m_width; x++) {
                uint8_t b = row[x * 3];
                uint8_t g = row[x * 3 + 1];
                uint8_t r = row[x * 3 + 2];
                m_RGBBytes.push_back(RGBValue(r, g, b));
            }
        }

        if (m_height > 0) {
            reverse(m_RGBBytes.begin(), m_RGBBytes.end()); // BMP stored bottom-up
        }
    }

    void readQOI(const string& filename, int channels=3, int colorspace=0) {
        m_QOIBytes = {};
        m_channels = channels;
        m_colorspace = colorspace;

        ifstream file(filename, ios::binary);
        if (!file) {
            cerr << "Failed to open QOI file for reading." << endl;
            return;
        }

        char magic[4];
        file.read(magic, 4);
        if (strncmp(magic, "qoif", 4) != 0) {
            cerr << "Invalid QOI magic." << endl;
            return;
        }

        file.read(reinterpret_cast<char*>(&m_width), 4);
        file.read(reinterpret_cast<char*>(&m_height), 4);
        file.read(reinterpret_cast<char*>(&m_channels), 1);
        file.read(reinterpret_cast<char*>(&m_colorspace), 1);

        m_QOIBytes.reserve(file.tellg());

        while (true) {
            int next = file.get();
            if (next == EOF) break;
            m_QOIBytes.push_back((uint8_t)next);

            // Check if the last 8 bytes match end marker
            if (m_QOIBytes.size() >= 8) {
                bool isEndMarker = true;
                for (int i = 0; i < 7; i++) {
                    if (m_QOIBytes[m_QOIBytes.size() - 8 + i] != 0) {
                        isEndMarker = false;
                        break;
                    }
                }
                if (isEndMarker && m_QOIBytes.back() == 1) {
                    // Remove end marker from QOIBytes
                    m_QOIBytes.resize(m_QOIBytes.size() - 8);
                    break;
                }
            }
        }
    }

    void writeBMP(const string& filename) {
        ofstream file(filename, ios::binary);
        if (!file) {
            cerr << "Failed to open BMP file for writing." << endl;
            return;
        }

        uint32_t rowPadded = (m_width * 3 + 3) & (~3);
        uint32_t fileSize = 54 + rowPadded * m_height;  // 54 = header size

        // --- BMP HEADER ---
        uint8_t header[54] = {
            'B', 'M',                   // Signature
            0,0,0,0,                     // File size
            0,0,0,0,                     // Reserved
            54,0,0,0,                    // Offset to pixel data
            40,0,0,0,                    // DIB header size
            0,0,0,0,                     // Width
            0,0,0,0,                     // Height
            1,0,                         // Planes
            24,0,                        // Bits per pixel
            0,0,0,0,                     // Compression
            0,0,0,0,                     // Image size (can be 0 for uncompressed)
            0,0,0,0,                     // X pixels per meter
            0,0,0,0,                     // Y pixels per meter
            0,0,0,0,                     // Colors in color table
            0,0,0,0                      // Important color count
        };

        // Set file size
        header[2] = (uint8_t)(fileSize);
        header[3] = (uint8_t)(fileSize >> 8);
        header[4] = (uint8_t)(fileSize >> 16);
        header[5] = (uint8_t)(fileSize >> 24);

        // Set width
        header[18] = (uint8_t)(m_width);
        header[19] = (uint8_t)(m_width >> 8);
        header[20] = (uint8_t)(m_width >> 16);
        header[21] = (uint8_t)(m_width >> 24);

        // Set height
        header[22] = (uint8_t)(m_height);
        header[23] = (uint8_t)(m_height >> 8);
        header[24] = (uint8_t)(m_height >> 16);
        header[25] = (uint8_t)(m_height >> 24);

        file.write(reinterpret_cast<char*>(header), 54);

        // --- PIXEL DATA ---
        vector<uint8_t> row(rowPadded, 0);
        for (int y = m_height - 1; y >= 0; --y) { // BMP stores bottom-up
            for (uint32_t x = 0; x < m_width; ++x) {
                const RGBValue& px = m_RGBBytes[y * m_width + (m_width - x - 1)];
                row[x * 3 + 0] = px.blue;
                row[x * 3 + 1] = px.green;
                row[x * 3 + 2] = px.red;
            }
            file.write(reinterpret_cast<char*>(row.data()), rowPadded);
        }
    }

    void writeQOI(const string& filename) {
        ofstream file(filename, ios::binary);
        if (!file) {
            cerr << "Failed to open QOI file for writing." << endl;
            return;
        }

        file.write("qoif", 4);
        file.write(reinterpret_cast<char*>(&m_width), 4);
        file.write(reinterpret_cast<char*>(&m_height), 4);
        file.write(reinterpret_cast<char*>(&m_channels), 1);
        file.write(reinterpret_cast<char*>(&m_colorspace), 1);

        // Write data chunks
        for (const auto& byte : m_QOIBytes) {
            file.put(byte);
        }

        // 8-byte end marker
        uint8_t endMarker[8] = {0, 0, 0, 0, 0, 0, 0, 1};
        file.write(reinterpret_cast<char*>(endMarker), 8);
    }

    vector<RGBValue> getRAW(bool print=false) {
        if (print) {
            for (RGBValue i : m_RGBBytes) {
                i.print();
            }   
            cout << "-------------------------" << endl;
            cout << "RAW Length: " << m_RGBBytes.size() << " bytes" << endl;
        }

        return m_RGBBytes;
    }

    vector<uint8_t> getQOI(bool print=false) {
        if (print) {
            for (auto i : m_QOIBytes) {
                cout << bitset<8>(i).to_string() << endl;
            }
            cout << "-------------------------" << endl;
            cout << "QOI Length: " << m_QOIBytes.size() << " bytes" << endl;
        }

        return m_QOIBytes;
    }

    void encode(bool verbose=false) {
        size_t curIdx = 0;
        RGBValue prevPixel;
        m_QOIBytes.reserve(m_RGBBytes.size()*3);
    
        while (curIdx < m_RGBBytes.size()) {
            // 1. check if it is same as previous pixel 
            if (curIdx > 0 && m_RGBBytes[curIdx] == prevPixel) {
                // if so, advance through image to look for length of run
                uint8_t runLength = 0;
                do {
                    runLength++;
                    curIdx++;
                    if (runLength >= 62) // runLengths of 1..62 are allowed
                        break;
                } while (m_RGBBytes[curIdx] == prevPixel); // curIdx remains unprocessed, no need to update prevPixel
    
                m_QOIBytes.push_back(0b11000000 + runLength); // (QOI_OP_RUN)
    
                if (curIdx >= m_RGBBytes.size())
                    break;
            }
    
            // 2. try to express as difference from previous 
            int dr = (int)m_RGBBytes[curIdx].red - (int)prevPixel.red; 
            int dg = (int)m_RGBBytes[curIdx].green - (int)prevPixel.green;
            int db = (int)m_RGBBytes[curIdx].blue - (int)prevPixel.blue;
            
            if ((-2<=dr && dr<=1) && (-2<=dg && dg<=1) && (-2<=db && db<=1)) { 
                m_QOIBytes.push_back(0b01000000 + ((dr+2)<<4) + ((dg+2)<<2) + (db+2)); // (QOI_OP_DIFF)
                
                prevPixel = m_RGBBytes[curIdx];
                curIdx++;
                continue;
            }
            else if ((-32<=dg && dg<=31) && (-8<=(dr-dg) && (dr-dg)<=7) && (-8<=(db-dg) && (db-dg)<=7)) {
                m_QOIBytes.push_back(0b10000000 + dg+32); // (QOI_OP_LUMA)
                m_QOIBytes.push_back(((dr-dg+8)<<4 )+ (db-dg+8)); 
                
                prevPixel = m_RGBBytes[curIdx];
                curIdx++;
                continue;
            }

            // 3. check index array
            uint8_t hash = m_RGBBytes[curIdx].hash();
            if (index[hash].isNull) { // hash is not in index
                index[hash] = m_RGBBytes[curIdx];
                m_QOIBytes.push_back(hash); // (QOI_OP_INDEX)
                
                prevPixel = m_RGBBytes[curIdx];
                curIdx++;
                continue;
            } 
            else if (index[hash] == m_RGBBytes[curIdx]) { // we can reuse index
                m_QOIBytes.push_back(hash); // (QOI_OP_INDEX)
    
                prevPixel = m_RGBBytes[curIdx];
                curIdx++;
                continue;
            }
    
            // 4. last resort: store full RGBValue (QOI_OP_RGB)
            m_QOIBytes.push_back(0b11111110);
            m_QOIBytes.push_back(m_RGBBytes[curIdx].red);
            m_QOIBytes.push_back(m_RGBBytes[curIdx].green);
            m_QOIBytes.push_back(m_RGBBytes[curIdx].blue);
    
            prevPixel = m_RGBBytes[curIdx];
            curIdx++;
        }

        if (verbose) {
            cout << "Original size:   " << (double)m_RGBBytes.size()*3/1000000 << "MB" << endl;
            cout << "Compressed size: " << (double)m_QOIBytes.size()/1000000 << "MB" << endl;
            cout << "Compression Rate: " << (double)(m_RGBBytes.size()*3 - m_QOIBytes.size()) / (double)m_RGBBytes.size() / 3 * 100 << "%" << endl;
        }
    }
    
    void decode() {
        m_RGBBytes = {};
        m_RGBBytes.reserve(m_width*m_height*3 + 9);
        size_t curIdx = 0;

        while (curIdx < m_QOIBytes.size()) {
            uint8_t curByte = m_QOIBytes[curIdx++];

            // QOI_OP_RGB
            if (curByte == 0b11111110) {
                uint8_t r = m_QOIBytes[curIdx++];
                uint8_t g = m_QOIBytes[curIdx++];
                uint8_t b = m_QOIBytes[curIdx++];
                
                RGBValue pixel(r, g, b);
                m_RGBBytes.push_back(pixel);
                continue;
            }

            // QOI_OP_INDEX
            if (curByte >> 6 == 0b00) {
                RGBValue pixel = index[curByte];
                m_RGBBytes.push_back(pixel);
                continue;
            } 

            // QOI_OP_DIFF
            if (curByte >> 6 == 0b01) {
                int dr = ((curByte >> 4) & 0b11) - 2;
                int dg = ((curByte >> 2) & 0b11) - 2;
                int db = (curByte & 0b11) - 2;

                RGBValue pixel(m_RGBBytes.back().red + dr, m_RGBBytes.back().green + dg, m_RGBBytes.back().blue + db);
                m_RGBBytes.push_back(pixel);
                continue;
            } 

            // QOI_OP_LUMA
            if (curByte >> 6 == 0b10) {
                uint8_t b2 = m_QOIBytes[curIdx++];
                int dg = (curByte & 0b111111) - 32;
                int dr_dg = ((b2 >> 4) & 0b1111) - 8;
                int db_dg = (b2 & 0b1111) - 8;
                int dr = dr_dg + dg;
                int db = db_dg + dg;

                RGBValue pixel(m_RGBBytes.back().red + dr, m_RGBBytes.back().green + dg, m_RGBBytes.back().blue + db);
                m_RGBBytes.push_back(pixel);
                continue;
            } 
            
            // QOI_OP_RUN
            if (curByte >> 6 == 0b11) {
                uint8_t run = curByte & 0b111111;
                for (int i=0; i<run; i++) {
                    m_RGBBytes.push_back(m_RGBBytes.back());
                }
            }
        }
    }
};



int main() {
    QOI_Image img;

    auto start = chrono::high_resolution_clock::now();
    
    img.readBMP("./images/sample_1920.bmp");
    auto t1 = chrono::high_resolution_clock::now();
    
    img.encode(true);
    auto t2 = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(t2 - t1);
    cout << "Time taken (encoding): " << duration.count() << "ms" << endl;
    
    img.writeQOI("./images/sample_1920.qoi");
    auto t3 = chrono::high_resolution_clock::now();
    
    img.readQOI("./images/sample_1920.qoi");
    auto t4 = chrono::high_resolution_clock::now();

    img.decode();
    auto t5 = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::milliseconds>(t5 - t4);
    cout << "Time taken (decoding): " << duration.count() << "ms" << endl;
    
    img.writeBMP("./images/sample_1920_NEW.bmp");
}