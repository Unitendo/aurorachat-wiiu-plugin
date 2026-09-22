#include "config.h"

#include <cstdio>
#include <cstring>

namespace AuroraChat {

    namespace {
        constexpr const char *accountFile = "fs:/vol/external01/wiiu/apps/aurorachatforWiiU/account.dat";
    } // namespace

    bool LoadLogin(std::string &username, std::string &password) {
        FILE *file = fopen(accountFile, "r");
        if (!file)
            return false;

        char user[128];
        char pass[128];

        if (!fgets(user, sizeof(user), file) || !fgets(pass, sizeof(pass), file)) {
            fclose(file);
            return false;
        }

        fclose(file);

        // remove newline
        user[strcspn(user, "\n")] = 0;
        pass[strcspn(pass, "\n")] = 0;

        username = user;
        password = pass;

        return true;
    }

} // namespace AuroraChat
