#ifndef tgl_reference_h
#define tgl_reference_h

#include "common.h"
#include "string"
#include "scene_distributed.h"

// reference
struct id_reference {
    // possible types of jsonvalue
    enum _Type { nullt, Materialt, Materialptrt, FrameAnimationptrt, Camerat, Lightt, Mesht};
    _Type _type = nullt;    // current type
    union {
        Material*        _m;     // Material ref
        Material**        _mptr;     // Material ref
        FrameAnimation*  _fa;    // FrameAnimation ref
        Camera*          _c;     // Camera ref
        Light*           _l;     // Lights ref
        Mesh*            _me;    // mesh ref
    };
    timestamp_t _id = 0;
    
    // constructor
    id_reference() : _type(nullt) { }
    
    // value constructors
    explicit id_reference(Material* m, timestamp_t ref_id) : _type(Materialt), _m(m), _id(ref_id) { }
    explicit id_reference(Material** mptr, timestamp_t ref_id) : _type(Materialptrt), _mptr(mptr), _id(ref_id) { }
    explicit id_reference(FrameAnimation* fa, timestamp_t ref_id) : _type(FrameAnimationptrt), _fa(fa), _id(ref_id) { }
    explicit id_reference(Camera* c, timestamp_t ref_id) : _type(Camerat), _c(c), _id(ref_id) { }
    explicit id_reference(Light* l, timestamp_t ref_id) : _type(Lightt), _l(l), _id(ref_id) { }
    explicit id_reference(Mesh* me, timestamp_t ref_id) : _type(Mesht), _me(me), _id(ref_id) { }

    // copy constructor
    id_reference(const id_reference& j) : _type(nullt) { set(j); }
    
    // destuctor
    ~id_reference() { _clear(); }
    
    // assignment
    id_reference& operator=(const id_reference& j) { set(j); return *this; }
    
    // clear
    void _clear() {
        if(_type==Materialt) delete _m;
        if(_type==Materialptrt) delete _mptr;
        if(_type==FrameAnimationptrt) delete _fa;
        if(_type==Camerat) delete _c;
        if(_type==Lightt) delete _l;
        if(_type==Mesht) delete _me;
        _type = nullt;
        _id = 0;
    }
    
    // set
    void set(const id_reference& j) {
        if(_type != nullt) _clear();
        _type = j._type;
        switch(_type) {
            case nullt: break;
            case Materialt: _m = j._m; break;
            case Materialptrt: _mptr = j._mptr; break;
            case FrameAnimationptrt: _fa = j._fa; break;
            case Camerat: _c = j._c; break;
            case Lightt: _l = j._l; break;
            case Mesht: _me = j._me; break;
            default: error("wrong type");
        }
        _id = j._id;
    }
    
    // get type as string
    string get_type() {
        switch(_type) {
            case nullt: return nullptr;
            case Materialt: return "material";
            case Materialptrt: return "material*";
            case FrameAnimationptrt: return "animation";
            case Camerat: return "camera";
            case Lightt: return "light";
            case Mesht: return "mesh";
            default: error("wrong type");
        }
    }
    
    // type checking
    bool is_null() const { return _type == nullt; }
    bool is_material() const { return _type == Materialt; }
    bool is_materialptr() const { return _type == Materialptrt; }
    bool is_animation() const { return _type == FrameAnimationptrt; }
    bool is_camera() const { return _type == Camerat; }
    bool is_light() const { return _type == Lightt; }
    bool is_mesh() const { return _type == Mesht; }

    // getters for values
    Material* as_material() const { error_if_not(is_material(), "wrong type"); return _m; }
    Material** as_materialptr() const { error_if_not(is_materialptr(), "wrong type"); return _mptr; }
    FrameAnimation* as_animation() const { error_if_not(is_animation(), "wrong type"); return _fa; }
    Camera* as_camera() const { error_if_not(is_camera(), "wrong type"); return _c; }
    Light* as_light() const { error_if_not(is_light(), "wrong type"); return _l; }
    Mesh* as_mesh() const { error_if_not(is_mesh(), "wrong type"); return _me; }

};

#endif
