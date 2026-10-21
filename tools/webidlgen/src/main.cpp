// Command line front end of the TinyBrowser WebIDL compiler.
//
//   webidlgen --header <out.h> --source <out.cpp> <idl> [<idl> ...]
//   webidlgen <idl> <out.h> <out.cpp>          (legacy positional form)

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "CodeGenerator.h"
#include "IdlParser.h"

namespace {

void printUsage(const char* program) {
    std::cout << "usage: " << program
              << " --header <out.h> --source <out.cpp> <idl> [<idl> ...]\n"
              << "       " << program << " <idl> <out.h> <out.cpp>\n";
}

bool readFile(const std::string& path, std::string& contents) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    contents = buffer.str();
    return true;
}

std::string baseName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return path;
    return path.substr(slash + 1);
}

// Merges the definitions of `source` into `target`.
void mergeDatabase(webidlgen::Database& target, const webidlgen::Database& source) {
    for (const webidlgen::Interface& iface : source.interfaces) {
        webidlgen::Interface* existing = nullptr;
        for (webidlgen::Interface& candidate : target.interfaces) {
            if (candidate.name == iface.name) {
                existing = &candidate;
                break;
            }
        }
        if (existing == nullptr) {
            target.interfaces.push_back(iface);
            continue;
        }
        if (existing->parent.empty()) existing->parent = iface.parent;
        for (const webidlgen::Member& member : iface.members) {
            existing->members.push_back(member);
        }
        for (const webidlgen::ExtendedAttribute& attribute : iface.extended) {
            existing->extended.push_back(attribute);
        }
    }

    for (const webidlgen::Dictionary& dict : source.dictionaries) {
        bool merged = false;
        for (webidlgen::Dictionary& candidate : target.dictionaries) {
            if (candidate.name != dict.name) continue;
            for (const webidlgen::Member& member : dict.members) {
                candidate.members.push_back(member);
            }
            merged = true;
            break;
        }
        if (!merged) target.dictionaries.push_back(dict);
    }

    for (const webidlgen::Enum& enumeration : source.enums) {
        bool merged = false;
        for (webidlgen::Enum& candidate : target.enums) {
            if (candidate.name != enumeration.name) continue;
            for (const std::string& value : enumeration.values) {
                candidate.values.push_back(value);
            }
            merged = true;
            break;
        }
        if (!merged) target.enums.push_back(enumeration);
    }

    for (const webidlgen::Callback& callback : source.callbacks) {
        target.callbacks.push_back(callback);
    }

    for (const webidlgen::Typedef& alias : source.typedefs) {
        target.typedefs.push_back(alias);
    }

    for (const std::string& diagnostic : source.diagnostics) {
        target.diagnostics.push_back(diagnostic);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string headerPath;
    std::string sourcePath;
    std::vector<std::string> idlFiles;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if (argument == "--header" && i + 1 < argc) {
            headerPath = argv[++i];
            continue;
        }
        if (argument == "--source" && i + 1 < argc) {
            sourcePath = argv[++i];
            continue;
        }
        if (argument.rfind("--", 0) == 0) {
            std::cerr << "webidlgen: unknown option '" << argument << "'\n";
            printUsage(argv[0]);
            return 2;
        }
        idlFiles.push_back(argument);
    }

    // Legacy positional form: <idl> <out.h> <out.cpp>.
    if (headerPath.empty() && sourcePath.empty() && idlFiles.size() == 3) {
        headerPath = idlFiles[1];
        sourcePath = idlFiles[2];
        idlFiles.resize(1);
    }

    if (headerPath.empty() || sourcePath.empty() || idlFiles.empty()) {
        printUsage(argv[0]);
        return 2;
    }

    webidlgen::Database database;
    bool failed = false;

    for (const std::string& path : idlFiles) {
        std::string contents;
        if (!readFile(path, contents)) {
            std::cerr << "webidlgen: cannot read '" << path << "'\n";
            failed = true;
            continue;
        }

        webidlgen::Parser parser(contents, path);
        webidlgen::Database parsed;
        if (!parser.parse(parsed)) {
            for (const std::string& error : parser.errors()) {
                std::cerr << "webidlgen: " << error << "\n";
            }
            failed = true;
            continue;
        }
        mergeDatabase(database, parsed);
    }

    if (failed) {
        std::cerr << "webidlgen: failed to parse the input files\n";
        return 1;
    }

    webidlgen::GeneratorOptions options;
    options.headerFileName = baseName(headerPath);
    options.sources = idlFiles;

    webidlgen::CodeGenerator generator(database, options);
    if (!generator.generate(headerPath, sourcePath)) {
        for (const std::string& diagnostic : generator.diagnostics()) {
            std::cerr << "webidlgen: " << diagnostic << "\n";
        }
        std::cerr << "webidlgen: failed to write the generated files\n";
        return 1;
    }

    for (const std::string& diagnostic : generator.diagnostics()) {
        std::cerr << "webidlgen: warning: " << diagnostic << "\n";
    }

    std::cout << "webidlgen: generated " << headerPath << " and " << sourcePath
              << " from " << idlFiles.size() << " WebIDL file(s), "
              << database.interfaces.size() << " interface(s)\n";
    return 0;
}
