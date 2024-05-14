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


bool check_credentials(const std::string& login, const std::string& pass) {
    // Normally you would query the database here
    return login == "admin" && pass == "password"; // Example check
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
        if (std::string(azColName[i]) == "question") {
            questions->push_back(argv[i] ? argv[i] : "NULL");
        }
    }
    return 0;
}

int main() {



    crow::SimpleApp app;





    CROW_ROUTE(app, "/edit-survey")
            ([]() {
                std::cout<<"IFUCKDOFUCKDOFUCKDOFUCKDOFUCKDOFUCKDO"<<std::endl;
                std::ifstream file("survey_form.html");
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                crow::mustache::context ctx;
                return crow::mustache::load("survey_form.html").render(ctx);
                //return crow::response{content};
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
                        size_t spacePos = entry.find(' ');
                        std::string qID = entry.substr(0, spacePos);
                        std::string questionText = escapeSQL(entry.substr(spacePos + 1));
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
                        size_t firstSpace = entry.find(' ');
                        size_t secondSpace = entry.find(' ', firstSpace + 1);
                        size_t thirdSpace = entry.find(' ', secondSpace + 1);
                        std::string aID = entry.substr(0, firstSpace);
                        std::string qID = entry.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                        std::string answerText = escapeSQL(entry.substr(thirdSpace + 1));
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

                int rc = sqlite3_open("mydatabase1.db", &db);
                if (rc) {
                    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;

                }

                std::vector<std::string> questions;
                const char* sql = "SELECT question FROM surveys;";
                sqlite3_exec(db, sql, callback_21, &questions, &zErrMsg);

                std::ostringstream os;
                os << "<html><head><title>Survey Questions</title></head><body>";
                os << "<h1>Survey Questions</h1>";
                os << "<ul>";
                for (const auto& question : questions) {
                    os << "<li>" << question << "</li>";
                }
                os << "</ul>";
                os << "</body></html>";

                return crow::response{os.str()};
            });

    app.port(8080).multithreaded().run();
    return 0;
}
