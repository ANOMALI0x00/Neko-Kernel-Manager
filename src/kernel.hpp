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
    bool has_files;   // NUEVO: indica si los archivos del kernel existen en /boot y /lib/modules
};

class Kernel {
public:
    Kernel(Package pkg, Package headers);
    std::string version() const;
    bool is_installed() const;
    bool has_files() const;          // NUEVO: consulta si los archivos existen
    bool install();
    bool remove();
    std::string category() const;
    std::string size() const;
    std::string installDate() const;
    std::string name() const { return m_pkg.name; }
    std::string type() const { return m_pkg.type; }
    void set_installed(bool inst) { m_pkg.is_installed = inst; }
    void set_has_files(bool has) { m_pkg.has_files = has; } // NUEVO: setter para has_files

    static std::vector<Kernel> getKernels();

private:
    Package m_pkg;
    Package m_headers;
};

#endif // KERNEL_HPP