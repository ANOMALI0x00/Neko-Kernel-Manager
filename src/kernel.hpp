#ifndef KERNEL_HPP
#define KERNEL_HPP

#include <string>
#include <vector>

struct Package {
    std::string name;
    std::string version;
    std::string repo;
    std::string type; // "xbps" or "manual"
    bool is_installed;
};

class Kernel {
public:
    Kernel(Package pkg, Package headers);
    std::string version() const;
    bool is_installed() const;
    bool install();
    bool remove();
    std::string category() const;
    std::string size() const;
    std::string installDate() const;
    std::string name() const { return m_pkg.name; }

    static std::vector<Kernel> getKernels();

private:
    Package m_pkg;
    Package m_headers;
};

#endif // KERNEL_HPP