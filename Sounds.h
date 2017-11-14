#include <vector> //needed to keep track of all buffers :/

class Sound{
public:
  static void* device;
  static void* context;
  static std::vector<unsigned int> buffers;
  static std::vector<unsigned int> sources;
  bool looping;
  static void init(int argc, char **argv);
  static void cleanup();
  Sound(){ } //default initializer does nothing
  Sound(const char* filename, bool loop = false);
  void play(bool blocking = false);
private:
  unsigned int source;
  unsigned int buffer;
  bool checkError();
};
