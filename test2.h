struct BaseChar {
  char f_char; };

struct BaseInt {
  int f_int; };

struct DerivedChar1 : BaseChar {
  int f_int; };

struct DerivedChar2 : BaseChar {
  char f_char2;
  int f_int; };

struct DerivedChar3 : BaseChar {
  int f_int;
  char f_char2; };

struct DerivedCI1 : BaseChar, BaseInt {
  int f_int;
  char f_char2; };

struct DerivedIC1 : BaseInt, BaseChar {
  char f_char2;
  int f_int; };

struct Virtual {
  short int f_sint;
  virtual void func(); };

struct DerivedVirtual1 : Virtual {
  short int f_sint2; };

struct DerivedVirtual2 : Virtual {
  int f_int; };

struct DerivedVirtualMix1 : DerivedVirtual1, DerivedVirtual2 {
  char f_char;
};

struct VD1 : virtual BaseChar {
  char f_char2;
};

struct VD2 : virtual BaseChar {
  int f_int;
};

struct VD : VD1, VD2 {
  float f_float;
};