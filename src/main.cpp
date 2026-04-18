#include <iostream>

#include "starter/app/application.hpp"
#include "starter/core/project_info.hpp"

int main(int argc, char** argv) {
    starter::Application application(starter::load_project_info(), std::cout, std::cerr);
    return application.run(argc, argv);
}

