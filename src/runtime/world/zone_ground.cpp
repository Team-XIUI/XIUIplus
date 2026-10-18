#include "zone_ground.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_set>

// Terrain decoding adapted from PhoenixLighting's zone_dat.cpp (xim DAT format).
namespace xiui {
namespace {
using Bytes = std::vector<BYTE>;
using Matrix4 = std::array<float, 16>;
bool ReadFile(const std::filesystem::path& path,Bytes& out) {
    std::ifstream file(path,std::ios::binary); if (!file) return false;
    file.seekg(0,std::ios::end); const auto size=static_cast<size_t>(file.tellg()); file.seekg(0);
    out.resize(size); return size && file.read(reinterpret_cast<char*>(out.data()),size).good();
}
uint32_t U32(const Bytes& b,size_t at) { uint32_t v=0; if (at+4<=b.size()) memcpy(&v,b.data()+at,4); return v; }
uint16_t U16(const Bytes& b,size_t at) { uint16_t v=0; if (at+2<=b.size()) memcpy(&v,b.data()+at,2); return v; }
float F32(const Bytes& b,size_t at) { float v=0; if (at+4<=b.size()) memcpy(&v,b.data()+at,4); return v; }
std::string Name(const Bytes& b,size_t at,size_t length) {
    std::string s; for (size_t i=0;i<length && at+i<b.size();++i) s+=static_cast<char>(b[at+i]);
    while (!s.empty() && (s.back()==' ' || s.back()=='\0')) s.pop_back();
    return s;
}

struct FileTables {
    Bytes v,f;
    bool Load(const std::filesystem::path& root) {
        for (int table=1;table<=9;++table) {
            const auto prefix=table==1 ? root : root/("ROM"+std::to_string(table));
            const auto suffix=table==1 ? std::string() : std::to_string(table);
            Bytes vt,ft;
            if (!ReadFile(prefix/("VTABLE"+suffix+".DAT"),vt) || !ReadFile(prefix/("FTABLE"+suffix+".DAT"),ft)) continue;
            // Each ROM's tables cover the same id space; set entries are combined.
            if (v.size()<vt.size()) v.resize(vt.size()); if (f.size()<ft.size()) f.resize(ft.size());
            for (size_t i=0;i<vt.size();++i) v[i]|=vt[i];
            for (size_t i=0;i<ft.size();++i) f[i]|=ft[i];
        }
        return !v.empty() && !f.empty();
    }
    std::filesystem::path Path(const std::filesystem::path& root,uint32_t id) const {
        if (id>=v.size() || !v[id] || 2*size_t(id)+2>f.size()) return {};
        const auto value=U16(f,2*size_t(id));
        const auto rom=v[id]==1 ? std::string("ROM") : "ROM"+std::to_string(v[id]);
        return root/rom/std::to_string(value>>7)/(std::to_string(value&0x7F)+".DAT");
    }
};

// The zone key tables live in FFXiMain.dll, found by a big-endian marker scan from 0x30000.
bool KeyTables(const std::filesystem::path& root,std::array<BYTE,256>& first,std::array<BYTE,256>& second) {
    Bytes dll; if (!ReadFile(root/"FFXiMain.dll",dll)) return false;
    auto find=[&](uint32_t hint,std::array<BYTE,256>& out) {
        for (size_t i=0,at=0x30000;i<0xC000 && at+4<=dll.size();++i,at+=4) {
            const uint32_t value=(uint32_t(dll[at])<<24)|(uint32_t(dll[at+1])<<16)|(uint32_t(dll[at+2])<<8)|dll[at+3];
            if (value==hint) { if (at+256>dll.size()) return false; memcpy(out.data(),dll.data()+at,256); return true; }
        }
        return false;
    };
    return find(0xE2E506A9u,first) && find(0xB8C5F784u,second);
}

struct Section { size_t start=0,size=0; uint32_t type=0; };

// xim ZoneInteractionSection and ZoneIdToResourceId: RID entries beginning with 'm' select a subarea DAT.
void SubareaFiles(const Bytes& dat,const Section& section,std::vector<uint32_t>& files) {
    const size_t begin=section.start+0x10,end=section.start+section.size;
    if (section.size<0x24 || !Name(dat,begin,4).starts_with("RID")) return;
    const size_t entries=begin+U32(dat,begin+0x10);
    if (entries<begin || entries+0x10>end) return;
    const size_t count=std::min<size_t>(U32(dat,entries),(end-entries-0x10)/0x40);
    for (size_t i=0;i<count;++i) {
        const size_t at=entries+0x10+i*0x40;
        const uint32_t id=U32(dat,at+0x2C);
        if (!Name(dat,at+0x24,4).starts_with('m') || !id || id>0xFFFF) continue;
        const uint32_t file=id<0x271 ? id+0x64 : 0x14768+(id-0x271);
        if (std::find(files.begin(),files.end(),file)==files.end()) files.push_back(file);
    }
}

void DecryptObjects(Bytes& b,const Section& s,const std::array<BYTE,256>& table) {
    const uint32_t meta=U32(b,s.start+0x10);
    int length=static_cast<int>(meta&0xFFFFFF); const uint32_t mode=meta>>24;
    if (mode<=0x1A) return;
    const uint32_t keyNode=U32(b,s.start+0x14);
    const uint32_t nodes=keyNode&0xFFFFFF;
    int key=table[((keyNode>>24)&0xFF)^0xFF],counter=0,consumed=0;
    size_t at=s.start+0x18;
    const size_t end=s.start+s.size;
    if (at+length>end) length=static_cast<int>(end-at);
    while (consumed<length) {
        const int span=((key>>4)&7)+16;
        if ((key&1) && consumed+span<length) for (int i=0;i<span;++i) b[at+i]^=0xFF;
        at+=span; key+=++counter; consumed+=span;
    }
    for (uint32_t i=0;i<nodes;++i) {
        const size_t name=s.start+0x30+size_t(i)*0x64;
        if (name+0x10>end) break;
        for (size_t j=0;j<0x10;++j) b[name+j]^=0x55;
    }
}
void DecryptMesh(Bytes& b,const Section& s,const std::array<BYTE,256>& first,const std::array<BYTE,256>& second) {
    const uint32_t meta=U32(b,s.start+0x10);
    const size_t total=meta&0xFFFFFF; if (total<8) return;
    size_t length=total-8; const size_t end=s.start+s.size;
    if (s.start+0x18+length>end) length=end-(s.start+0x18);
    const BYTE keyByte=b[s.start+0x15];
    if ((meta>>24)>=5) {
        int key=first[keyByte^0xF0],counter=0;
        for (size_t i=0;i<length;++i) {
            const int mod=((key&0xFF)<<8)|(key&0xFF);
            key+=++counter;
            b[s.start+0x18+i]^=static_cast<BYTE>(static_cast<unsigned>(mod)>>(key&7));
            key+=++counter;
        }
    }
    if (U16(b,s.start+0x16)!=0xFFFF) return;
    int key1=keyByte^0xF0,key2=second[key1];
    const size_t count=(length&~size_t(0xF))/2;
    for (size_t i=0,at=s.start+0x18;i<count;i+=8,at+=8) {
        if ((key2&1) && at+count+8<=end) for (size_t j=0;j<8;++j) std::swap(b[at+j],b[at+count+j]);
        key1+=9; key2+=key1;
    }
}

struct ZoneVertex { float position[3]; };
struct SubMesh { std::vector<ZoneVertex> vertices; std::vector<uint16_t> indices; bool blend=false; };
struct MeshGroup { std::vector<SubMesh> parts; bool cutout=false; };

bool ParseMesh(const Bytes& b,const Section& s,std::string& name,MeshGroup& group) {
    const size_t end=s.start+s.size; size_t at=s.start+0x10;
    const uint32_t config=U32(b,at+4)&0xFF; at+=0x10;
    const bool strip=(config&1)!=0,blendPositions=(config&2)!=0;
    name=Name(b,at,0x10); at+=0x10;
    group.cutout=!name.empty() && name[0]=='_';
    const size_t definition=at;
    if (!U32(b,at)) return false;
    at+=4+24; const size_t data=definition+U32(b,at); at+=4;
    const uint32_t meshes=U32(b,at); at+=4+24+4;
    if (at!=data || meshes>4096) return false;
    for (uint32_t m=0;m<meshes;++m) {
        SubMesh part; at+=0x10;
        const uint16_t count=U16(b,at),flags=U16(b,at+2); at+=4;
        part.blend=(flags&0x8000)!=0;
        const size_t stride=blendPositions ? 48 : 36;
        if (at+count*stride>end) return false;
        part.vertices.resize(count);
        for (uint16_t v=0;v<count;++v,at+=stride) {
            auto& out=part.vertices[v];
            memcpy(out.position,b.data()+at,12);
        }
        const uint16_t indexCount=U16(b,at); at+=4;
        if (at+size_t(indexCount)*2>end) return false;
        std::vector<uint16_t> raw(indexCount);
        for (uint16_t i=0;i<indexCount;++i) { raw[i]=U16(b,at); at+=2; if (raw[i]>=count) return false; }
        at=(at+3)&~size_t(3);
        if (!strip) { part.indices.assign(raw.begin(),raw.end()-raw.size()%3); }
        else for (size_t i=2;i<raw.size();++i) {
            const uint16_t a=raw[i-2],c=raw[i-1],d=raw[i];
            if (a==c || c==d || a==d) continue;
            if (i&1) part.indices.insert(part.indices.end(),{c,a,d}); else part.indices.insert(part.indices.end(),{a,c,d});
        }
        group.parts.push_back(std::move(part));
    }
    return true;
}

struct Object { std::string id; float position[3],rotation[3],scale[3]; bool dynamic=false; };
std::vector<Object> ParseObjects(const Bytes& b,const Section& s) {
    std::vector<Object> objects;
    const uint32_t nodes=U32(b,s.start+0x14)&0xFFFFFF;
    const size_t end=s.start+s.size;
    for (uint32_t i=0;i<nodes;++i) {
        const size_t at=s.start+0x30+size_t(i)*0x64;
        if (at+0x64>end) break;
        Object o; o.id=Name(b,at,0x10);
        for (int c=0;c<3;++c) { o.position[c]=F32(b,at+0x10+c*4); o.rotation[c]=F32(b,at+0x1C+c*4); o.scale[c]=F32(b,at+0x28+c*4); }
        o.dynamic=U32(b,at+0x34)!=0; // effect-linked doors/elevators move at runtime
        objects.push_back(std::move(o));
    }
    return objects;
}

// xim: translate(position) * rotateZYX(rotation) * scale(scale), column-major; that layout is our row-vector matrix.
Matrix4 ObjectMatrix(const Object& o) {
    const float sx=std::sin(o.rotation[0]),sy=std::sin(o.rotation[1]),sz=std::sin(o.rotation[2]);
    const float cx=std::cos(o.rotation[0]),cy=std::cos(o.rotation[1]),cz=std::cos(o.rotation[2]);
    const float r[9]{cy*cz,cy*sz,-sy, sx*sy*cz-cx*sz,sx*sy*sz+cx*cz,sx*cy, cx*sy*cz+sx*sz,cx*sy*sz-sx*cz,cx*cy};
    Matrix4 m{};
    for (int axis=0;axis<3;++axis) for (int c=0;c<3;++c) m[axis*4+c]=r[axis*3+c]*o.scale[axis];
    m[12]=o.position[0]; m[13]=o.position[1]; m[14]=o.position[2]; m[15]=1;
    return m;
}
}

namespace {
uint64_t Cell(int x, int z) { return (uint64_t(uint32_t(x)) << 32) | uint32_t(z); }

void LoadGroundFile(uint32_t file, const std::filesystem::path& root, const FileTables& tables,
    const std::array<BYTE,256>& first, const std::array<BYTE,256>& second,
    const std::atomic_bool& cancel, std::unordered_set<uint32_t>& visited, zone_ground& ground) {
    if (cancel || !visited.insert(file).second || visited.size() > 32) return;
    Bytes dat;
    if (!ReadFile(tables.Path(root, file), dat)) return;
    struct wipe { Bytes& data; ~wipe() { SecureZeroMemory(data.data(), data.size()); } } cleanup{dat};
    std::unordered_map<std::string, MeshGroup> meshes;
    std::vector<Object> objects;
    std::vector<uint32_t> children;
    unsigned effects = 0;
    std::vector<bool> directories;
    for (size_t at = 0; at + 16 <= dat.size() && !cancel;) {
        const auto meta = U32(dat, at + 4);
        Section s{at, size_t((meta >> 7) & 0xFFFFF) * 16, meta & 0x7F};
        if (s.size < 16 || s.size > dat.size() - at) break;
        if (s.type == 1) { const bool effect = Name(dat, at, 4) == "effe"; directories.push_back(effect); effects += effect; }
        else if (s.type == 0 && !directories.empty()) { effects -= directories.back(); directories.pop_back(); }
        else if (!effects && s.type == 0x2E && s.size >= 0x20) {
            DecryptMesh(dat, s, first, second);
            std::string name; MeshGroup mesh;
            if (ParseMesh(dat, s, name, mesh)) meshes[name] = std::move(mesh);
        } else if (!effects && s.type == 0x1C && s.size >= 0x30) {
            DecryptObjects(dat, s, first);
            auto parsed = ParseObjects(dat, s); objects.insert(objects.end(), parsed.begin(), parsed.end());
        } else if (s.type == 0x36) SubareaFiles(dat, s, children);
        at += s.size;
    }
    for (const auto& object : objects) {
        if (cancel) return;
        if (object.dynamic) continue;
        bool finite = true;
        for (unsigned c = 0; c < 3; ++c) finite &= std::isfinite(object.position[c]) && std::isfinite(object.rotation[c]) && std::isfinite(object.scale[c]);
        if (!finite) continue;
        auto id = object.id;
        if (id.size() > 2 && id[id.size()-2] == '_' && (id.back() == 'l' || id.back() == 'm' || id.back() == 'h'))
            for (const char* suffix : {"_h", "_m", "_l"})
                if (meshes.contains(id.substr(0, id.size()-2) + suffix)) { id = id.substr(0, id.size()-2) + suffix; break; }
        const auto mesh = meshes.find(id);
        if (mesh == meshes.end() || mesh->second.cutout) continue;
        const auto matrix = ObjectMatrix(object);
        for (const auto& part : mesh->second.parts) {
            if (part.blend) continue;
            for (size_t t = 0; t + 2 < part.indices.size(); t += 3) {
                ground_triangle triangle{};
                for (unsigned v = 0; v < 3; ++v) {
                    const auto* p = part.vertices[part.indices[t+v]].position;
                    for (unsigned c = 0; c < 3; ++c) triangle[v][c] = p[0]*matrix[c] + p[1]*matrix[4+c] + p[2]*matrix[8+c] + matrix[12+c];
                }
                ground.Add(triangle);
            }
        }
    }
    for (auto child : children) LoadGroundFile(child, root, tables, first, second, cancel, visited, ground);
}
}

auto zone_ground::Add(const ground_triangle& t) -> void {
    for (const auto& p : t) for (float v : p) if (!std::isfinite(v) || std::abs(v) > 32768) return;
    const float ax = t[1][0]-t[0][0], ay = t[1][1]-t[0][1], az = t[1][2]-t[0][2];
    const float bx = t[2][0]-t[0][0], by = t[2][1]-t[0][1], bz = t[2][2]-t[0][2];
    const float nx = ay*bz-az*by, ny = az*bx-ax*bz, nz = ax*by-ay*bx;
    // Exclude vertical walls and near-degenerate faces, while retaining walkable slopes.
    if (std::abs(ny) < 1e-6f || ny*ny < 0.12f*(nx*nx+ny*ny+nz*nz)) return;
    const int x0 = int(std::floor(std::min({t[0][0],t[1][0],t[2][0]})/cell_size));
    const int x1 = int(std::floor(std::max({t[0][0],t[1][0],t[2][0]})/cell_size));
    const int z0 = int(std::floor(std::min({t[0][2],t[1][2],t[2][2]})/cell_size));
    const int z1 = int(std::floor(std::max({t[0][2],t[1][2],t[2][2]})/cell_size));
    if (int64_t(x1-x0+1)*(z1-z0+1) > 65536) return;
    const auto index = static_cast<uint32_t>(this->triangles.size());
    this->triangles.push_back(t);
    for (int x=x0; x<=x1; ++x) for (int z=z0; z<=z1; ++z) this->cells[Cell(x,z)].push_back(index);
}

auto zone_ground::Nearby(const ground_point& p, float radius, std::vector<uint32_t>& indices) const -> void {
    indices.clear();
    for (int x=int(std::floor((p[0]-radius)/cell_size)); x<=int(std::floor((p[0]+radius)/cell_size)); ++x)
        for (int z=int(std::floor((p[2]-radius)/cell_size)); z<=int(std::floor((p[2]+radius)/cell_size)); ++z)
            if (const auto cell=this->cells.find(Cell(x,z)); cell!=this->cells.end()) indices.insert(indices.end(),cell->second.begin(),cell->second.end());
    std::sort(indices.begin(),indices.end()); indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
}

auto zone_ground::TriangleHeight(const ground_triangle& t, float x, float z, float& height) -> bool {
    const float area=(t[1][0]-t[0][0])*(t[2][2]-t[0][2])-(t[1][2]-t[0][2])*(t[2][0]-t[0][0]);
    if (std::abs(area)<1e-8f) return false;
    const float u=((t[1][0]-x)*(t[2][2]-z)-(t[1][2]-z)*(t[2][0]-x))/area;
    const float v=((t[2][0]-x)*(t[0][2]-z)-(t[2][2]-z)*(t[0][0]-x))/area;
    if (u < -1e-5f || v < -1e-5f || u+v > 1.00001f) return false;
    height=u*t[0][1]+v*t[1][1]+(1-u-v)*t[2][1]; return true;
}

auto zone_ground::Height(float x, float z, float anchor, float reach, float& height) const -> bool {
    const auto cell=this->cells.find(Cell(int(std::floor(x/cell_size)),int(std::floor(z/cell_size))));
    if (cell==this->cells.end()) return false;
    bool found=false;
    for (auto index : cell->second) {
        float y{};
        if (TriangleHeight(this->triangles[index],x,z,y) && std::abs(y-anchor)<reach) {
            reach=std::abs(y-anchor); height=y; found=true;
        }
    }
    return found;
}

auto LoadZoneGround(uint16_t zone, const std::filesystem::path& root, const std::atomic_bool& cancel) -> std::unique_ptr<zone_ground> {
    auto result=std::make_unique<zone_ground>();
    FileTables tables; std::array<BYTE,256> first{},second{};
    if (!tables.Load(root) || !KeyTables(root,first,second) || cancel) return result;
    std::unordered_set<uint32_t> visited;
    const uint32_t file=zone<0x100 ? 0x64+zone : 0x147B3+(zone-0x100);
    LoadGroundFile(file,root,tables,first,second,cancel,visited,*result);
    return result;
}
}
