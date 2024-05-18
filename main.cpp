/*#include "crow.h"
#include <string>
#include <fstream>


int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/edit-survey")
            ([]() {
                std::ifstream file("survey_form.html");
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                crow::mustache::context ctx;
                return crow::mustache::load("survey_form.html").render(ctx);
                //return crow::response{content};
            });

    CROW_ROUTE(app, "/submit-survey").methods(crow::HTTPMethod::Post)
            ([](const crow::request& req) {
                auto params = crow::query_string(req.body);
                std::ofstream file("/home/ivan/CLionProjects/untitled/survey_data.txt", std::ios::app); // Appends to a file
                file << params.get("question") << std::endl;
                std::cout<<params.get("question")<<std::endl;
                file.close();
                return crow::response{"Survey Saved "};
            });

    app.port(8080).multithreaded().run();
    return 0;
}*/
#include "crow.h"
#include "crow/mustache.h"

#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <sstream>
#include <sqlite3.h>

std::map<std::string, std::string> parse_cookies(const std::string& cookie_header) {
    std::map<std::string, std::string> cookies;
    std::stringstream ss(cookie_header);
    std::string cookie;
    while (std::getline(ss, cookie, ';')) {
        std::size_t separator = cookie.find('=');
        if (separator != std::string::npos) {
            std::string key = cookie.substr(0, separator);
            std::string value = cookie.substr(separator + 1);
            key.erase(0, key.find_first_not_of(" "));  // Trim leading spaces
            key.erase(key.find_last_not_of(" ") + 1);  // Trim trailing spaces
            value.erase(0, value.find_first_not_of(" "));  // Trim leading spaces
            value.erase(value.find_last_not_of(" ") + 1);  // Trim trailing spaces
            cookies[key] = value;
        }
    }
    return cookies;
}

bool is_authenticated(const crow::request& req) {
    auto cookie_header = req.get_header_value("Cookie");
    if (!cookie_header.empty()) {
        auto cookies = parse_cookies(cookie_header);
        auto it = cookies.find("auth");
        if (it != cookies.end() && it->second == "validated") {
            return true;
        }
    }
    return false;
}


bool check_credentials(const std::string& login, const std::string& pass) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    const char* dbPath = "mydatabase.db";

    // Open the database connection
    if (sqlite3_open(dbPath, &db) != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Prepare the SQL statement with parameter placeholder
    const char* sql = "SELECT PASS FROM AdminCredentials WHERE ADMINLOGIN = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        std::cerr << "Preparation failed: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Bind the login parameter to the SQL statement
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);

    // Execute the statement and check the password
    bool is_valid = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* dbPass = sqlite3_column_text(stmt, 0);
        if (dbPass && pass == reinterpret_cast<const char*>(dbPass)) {
            is_valid = true;
        }
    } else {
        std::cerr << "User not found or error executing statement: " << sqlite3_errmsg(db) << std::endl;
    }

    // Clean up
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return is_valid;
}


// Function to URL-decode a string
std::string urlDecode(const std::string& str) {
    std::string decoded;
    char ch;
    int i, ii;
    for (i = 0; i < str.length(); i++) {
        if (str[i] != '%') {
            if (str[i] == '+')
                decoded += ' ';
            else
                decoded += str[i];
        } else {
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            decoded += ch;
            i = i + 2;
        }
    }
    return decoded;
}




std::vector<std::string> extractMatches(const std::string& input) {
    // Regex pattern to find substrings for questionText[] and answer1[]
    std::regex pattern(R"((questionText%5B%5D|answer\d+%5B%5D)=([^&]*))");
    std::vector<std::string> results;

    auto begin = std::sregex_iterator(input.begin(), input.end(), pattern);
    auto end = std::sregex_iterator();
    int questCounter = 0;
    int ansCounter = 0;
    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::smatch match = *i;
        std::string decoded = urlDecode(match.str(2)); // Decode the captured group
        size_t matchPosition = match.position(2);

        std::string space = " ";
        if (input[matchPosition-19] == 'q') {
            questCounter++;
            results.push_back("q" + std::to_string(questCounter) + space + decoded);
            ansCounter = 0;
        }
        else
        {
            ansCounter++;
            results.push_back( "a" + std::to_string(ansCounter) + space + "q" + std::to_string(questCounter) + space + decoded);

        }


    }

    return results;
}


static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        // Check if the column name is 'name' which contains the table names
        if (std::string(azColName[i]) == "name") {
            std::cout << argv[i] << std::endl;
        }
    }
    return 0;
}

std::string escapeSQL(const std::string& s) {
    std::string escaped = s;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");  // Replace ' with ''
        pos += 2;  // Move past the newly inserted ''
    }
    return escaped;
}



int callback_21(void* data, int argc, char** argv, char** azColName) {
    auto* questions = static_cast<std::vector<std::string>*>(data);
    for (int i = 0; i < argc; i++) {
        if (std::string(azColName[i]) == "Text") {
            questions->push_back(argv[i] ? argv[i] : "NULL");
        }
    }
    return 0;
}


int callback_22(void* data, int argc, char** argv, char** azColName) {
    auto* questions = static_cast<std::vector<std::string>*>(data);
    for (int i = 0; i < argc; i++) {
        if (std::string(azColName[i]) == "AnswerText") {
            questions->push_back(argv[i] ? argv[i] : "NULL");
        }
    }
    return 0;
}
int main() {



    crow::SimpleApp app;




    CROW_ROUTE(app, "/login")
            ([] {
                crow::mustache::context ctx;
                return crow::mustache::load("login.html").render(ctx);
            });

    // Route to handle login attempt
    CROW_ROUTE(app, "/handle-login").methods("POST"_method)
            ([&](const crow::request& req) {
                std::string login;
                std::string pass;

                std::regex loginRegex(R"(login=([^&]*))");  // Regex to extract login
                std::regex passRegex(R"(pass=(.*))");       // Regex to extract password

                std::smatch match;

                // Extract login
                if (std::regex_search(req.body, match, loginRegex) && match.size() > 1) {
                    login = match[1].str();  // match[1] because match[0] is the whole matched text
                    std::cout << "Login: " << login << std::endl;
                }

                // Extract password
                if (std::regex_search(req.body, match, passRegex) && match.size() > 1) {
                    pass = match[1].str();  // match[1] for the captured group
                    std::cout << "Password: " << pass << std::endl;
                }






                // Debug output to console
                std::cout << "Login Attempt: " << login << " with pass: " << pass << std::endl;

                // Add database query logic here
                // For example, assuming a function check_credentials returns true if credentials are correct
                if (!login.empty() && !pass.empty() && check_credentials(login, pass)) {
                    crow::response res(302);
                    res.add_header("Location", "/edit-survey");
                    res.add_header("Set-Cookie", "auth=validated; Path=/; HttpOnly; SameSite=Strict");  // Update cookie policy
                    return res;
                } else {
                    crow::response res(302);
                    res.add_header("Location", "/login");
                    return res;  // Redirect to login on failure
                }
            });
    // Route for editing survey
            CROW_ROUTE(app, "/edit-survey")
            ([&](const crow::request& req) {
            std::string cookies = req.get_header_value("Cookie");
            auto parsed_cookies = parse_cookies(cookies);
            if (!is_authenticated(req)) {
                std::ifstream file("login.html");
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                crow::mustache::context ctx;
                return crow::mustache::load("login.html").render(ctx);
                }

            std::ifstream file("survey_form.html");
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            crow::mustache::context ctx;
            return crow::mustache::load("survey_form.html").render(ctx);
            });


CROW_ROUTE(app, "/submit-survey").methods(crow::HTTPMethod::Post)
            ([](const crow::request& req) {
                std::vector<std::string> matches = extractMatches(req.body);
                for (const auto& match : matches)
                {
                    std::cout << match << std::endl;
                }

                sqlite3 *db;
                char *errrm = 0;
                int rc;

                rc = sqlite3_open("mydatabase.db", &db);

                if( rc ){
                    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                    return crow::response(500, "Error: Could not open database");
                } else {
                    fprintf(stderr, "Opened database successfully\n");
                }

                /* Execute SQL statement */
//    rc = sqlite3_exec(db, "INSERT INTO Surveys (Title) VALUES ('Surgeons Trial Survey');", 0, 0, &errrm);
//
//    if( rc != SQLITE_OK ){
//        fprintf(stderr, "SQL error: %s\n", errrm);
//        sqlite3_free(errrm);
//    } else {
//        fprintf(stdout, "Records created successfully\n");
//    }
                std::string sql_qs = "BEGIN TRANSACTION;";
                std::map<std::string, int> questionIDs;
                int questionCounter = 1;

                for (const auto& entry : matches) {
                    if (entry[0] == 'q') {

                        std::string qID = entry.substr(1, 1);
                        std::string questionText = escapeSQL(entry.substr(3));
                        sql_qs += "INSERT INTO Questions (SurveyID, Text) VALUES (1, '" + questionText + "');";
                        questionIDs[qID] = questionCounter++; // Associate question text identifier with an ID
                    }
                }
                sql_qs += " COMMIT;";

                // Execute SQL for questions
                rc = sqlite3_exec(db, sql_qs.c_str(), 0, 0, &errrm);
                if (rc != SQLITE_OK) {
                    std::cerr << "SQL error Q: " << errrm << std::endl;
                    sqlite3_free(errrm);
                    errrm = 0;  // Reset error message pointer
                }

                std::string sql_ans = "BEGIN TRANSACTION;";
                for (const auto& entry : matches) {
                    if (entry[0] == 'a') {

                        std::string aID = entry.substr(1, 1);
                        std::string qID = entry.substr(4, 1);
                        std::string answerText = escapeSQL(entry.substr(6));
                        sql_ans += "INSERT INTO Answers (QuestionID, AnswerText) VALUES (" + std::to_string(questionIDs[qID]) + ", '" + answerText + "');";
                    }
                }
                sql_ans += " COMMIT;";

                // Execute SQL for answers
                rc = sqlite3_exec(db, sql_ans.c_str(), 0, 0, &errrm);
                if (rc != SQLITE_OK) {
                    std::cerr << "SQL error A: " << errrm << std::endl;
                    sqlite3_free(errrm);
                    return crow::response(500, "Server Error: Failed to insert data.");
                }



                sqlite3_close(db);


                return crow::response("Survey Saved");
            });

    CROW_ROUTE(app, "/surveys")
            ([]() {

                sqlite3* db;
                char* zErrMsg = 0;

                int rc = sqlite3_open("mydatabase.db", &db);
                if (rc) {
                    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;

                }

                std::vector<std::string> questions;
                std::string sql = "SELECT * FROM questions;";
                sqlite3_exec(db, sql.c_str(), callback_21, &questions, &zErrMsg);

                using QuestionAnswers = std::tuple<std::string, std::vector<std::string>>;

                QuestionAnswers questionAnswers;
                std::vector<QuestionAnswers> QAs;
                std::string semcol = ";";
                for(int i = 1; i <=questions.size();i++)
                {
                     std::vector<std::string> answers;
                     sql = "SELECT * FROM answers WHERE questionID = " + std::to_string(i) + ";";
                     sqlite3_exec(db, sql.c_str(), callback_22, &answers, &zErrMsg);
                     QuestionAnswers qa(questions[i-1], answers);
                     QAs.push_back(qa);
                }



                std::ostringstream os;
                os << "<html><head><title>Survey Questions</title></head><body>";
                os << "<h1>Survey Questions</h1>";
                os << "<ul>";
                for (const auto& question : QAs) {
                    os << "<li>" << std::get<0>(question)<< "</li>";  // assuming each question is paired with its answers
                    os << "<ul>";
                    for(const auto& answer: std::get<1>(question))  // each question has its specific list of answers
                    {
                        os << "<li>" << answer << "</li>";
                    }
                    os << "</ul>";
                }
                os << "</ul>";
                os << "</body></html>";

                return crow::response{os.str()};
            });

    app.port(8080).multithreaded().run();
    return 0;
}
