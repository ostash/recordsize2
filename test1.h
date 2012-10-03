namespace emptyns
{}

template <class T> class TemplateClass
{
 int f_int;
 T f_T;
};

class NonTemplateClass
{
  int f_int;
  double f_double;
};

class NonOptimal
{
  int f_int1;
  char f_char1;
  int f_int2;
  char f_char2;
};

class BitField
{
  int f_bit1 : 3;
  int d_bit2 : 17;
  char f_char;
};

void func()
{
  TemplateClass<char> c_1;
  TemplateClass<void*> c_2;
}

namespace ns1 {

  class NamespacedClass
  {
    float f_float;
    int f_int;
  };

  class AnonStructClass
  {
    int f_int;
    struct
    {
      char f_s_char;
      int f_s_int;
    };
  };

  namespace ns2
  {
    
    using namespace emptyns;

    class AnonUnionClass
    {
      char f_char;
      union {
        char f_u_char;
        int f_u_int;
      };
    };
  }
}

using ns1::NamespacedClass;
