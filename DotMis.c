/*
 *    DotMis by Plastic Bottleneck
 *    https://github.com/plastic-bottleneck/
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <math.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#endif

#define MAX_LINE 256

volatile sig_atomic_t interrupted = 0;  // Ctrl-C flag
int currentProgramLine = 0;               // For error reporting

void sigint_handler(int signum) {
    interrupted = 1;
}

typedef struct Line {
    int number;
    char *text;
    struct Line *next;
} Line;

Line *program = NULL;  // Program lines

long double numVars[26] = {0};
char *strVars[26] = {0};

#define GOSUB_STACK_SIZE 100
int gosubStack[GOSUB_STACK_SIZE];
int gosubStackTop = -1;

/* Forward declarations */
long double parseExpression(char **s);
long double parseTerm(char **s);
long double parseFactor(char **s);
void executeStatement(char *s, int *nextLine, int *jumpLine);
Line* findLine(int num);
Line* getNextLine(Line *current);
void runProgram(void);
void saveProgram(const char *filename);
void loadProgram(const char *filename);
void show_help_menu(void);
void trimTrailingSpaces(char *s);

/* Skip whitespace */
void skipWhitespace(char **s) {
    while ((*s)[0] != '\0' && isspace((*s)[0])) {
        (*s)++;
    }
}

/* Trim trailing spaces */
void trimTrailingSpaces(char *s) {
    int len = strlen(s);
    while(len > 0 && isspace(s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

/* Error reporting */
void error(const char *msg) {
    if (currentProgramLine != 0)
        fprintf(stderr, "Error at line %d: %s\n", currentProgramLine, msg);
    else
        fprintf(stderr, "Error: %s\n", msg);
}

/* Expression Parsing */
long double parseFactor(char **s) {
    skipWhitespace(s);
    long double result = 0;
    if ((*s)[0] == '(') {
        (*s)++;
        result = parseExpression(s);
        skipWhitespace(s);
        if ((*s)[0] == ')')
            (*s)++;
        else
            error("Missing )");
    } else if (isalpha((*s)[0])) {
        char ident[MAX_LINE];
        int i = 0;
        while (isalpha((*s)[0])) {
            ident[i++] = (*s)[0];
            (*s)++;
        }
        ident[i] = '\0';
        skipWhitespace(s);
        if ((*s)[0] == '(') {
            error("Unknown function");
            while ((*s)[0] && (*s)[0] != ')')
                (*s)++;
            if ((*s)[0] == ')')
                (*s)++;
            return 0;
        } else {
            if (strlen(ident) == 1) {
                char var = toupper(ident[0]);
                result = numVars[var - 'A'];
            } else {
                error("Unknown identifier");
            }
        }
    } else {
        char *end;
        errno = 0;
        result = strtold(*s, &end);
        if (errno != 0 || end == *s)
            error("Invalid number");
        *s = end;
    }
    skipWhitespace(s);
    return result;
}

long double parseTerm(char **s) {
    long double result = parseFactor(s);
    skipWhitespace(s);
    while ((*s)[0] == '*' || (*s)[0] == '/') {
        char op = (*s)[0];
        (*s)++;
        long double right = parseFactor(s);
        if (op == '*')
            result *= right;
        else {
            if (right == 0) { error("Division by zero"); result = 0; }
            else result /= right;
        }
        skipWhitespace(s);
    }
    return result;
}

long double parseExpression(char **s) {
    long double result = parseTerm(s);
    skipWhitespace(s);
    while ((*s)[0] == '+' || (*s)[0] == '-') {
        char op = (*s)[0];
        (*s)++;
        long double right = parseTerm(s);
        if (op == '+') result += right;
        else result -= right;
        skipWhitespace(s);
    }
    return result;
}

/* Program Storage */
void addProgramLine(int num, const char *text) {
    Line *prev = NULL, *curr = program;
    while (curr && curr->number < num) {
        prev = curr;
        curr = curr->next;
    }
    if (curr && curr->number == num) {
        free(curr->text);
        curr->text = strdup(text);
        return;
    }
    Line *newLine = malloc(sizeof(Line));
    if (!newLine) { perror("malloc"); exit(1); }
    newLine->number = num;
    newLine->text = strdup(text);
    newLine->next = curr;
    if (prev) prev->next = newLine;
    else program = newLine;
}

void deleteProgramLine(int num) {
    Line *prev = NULL, *curr = program;
    while (curr && curr->number != num) {
        prev = curr;
        curr = curr->next;
    }
    if (!curr) return;
    if (prev) prev->next = curr->next;
    else program = curr->next;
    free(curr->text);
    free(curr);
}

void listProgram(void) {
    Line *curr = program;
    while (curr) {
        printf("%d %s\n", curr->number, curr->text);
        curr = curr->next;
    }
}

void newProgram(void) {
    Line *curr = program;
    while (curr) {
        Line *next = curr->next;
        free(curr->text);
        free(curr);
        curr = next;
    }
    program = NULL;
}

void clearStringVars(void) {
    for (int i = 0; i < 26; i++) {
        if (strVars[i]) { free(strVars[i]); strVars[i] = NULL; }
    }
}

Line* findLine(int num) {
    Line *curr = program;
    while (curr) {
        if (curr->number == num) return curr;
        curr = curr->next;
    }
    return NULL;
}

Line* getNextLine(Line *current) {
    return current ? current->next : NULL;
}

/* PC Speaker Beep */
void beepCommand(int freq, int duration) {
#ifdef __linux__
    int console_fd = open("/dev/console", O_WRONLY);
    if (console_fd == -1) { perror("open /dev/console"); return; }
    if (ioctl(console_fd, KIOCSOUND, (int)(1193180 / freq)) < 0)
        perror("ioctl");
    usleep(duration * 1000);
    if (ioctl(console_fd, KIOCSOUND, 0) < 0)
        perror("ioctl");
    close(console_fd);
#else
    printf("\a"); fflush(stdout);
    usleep(duration * 1000);
#endif
}

/* SAVE and LOAD */
void saveProgram(const char *filename) {
    const char *ext = ".pbcb";
    size_t len = strlen(filename);
    char fullFilename[MAX_LINE];
    if (len < strlen(ext) || strcmp(filename + len - strlen(ext), ext) != 0)
        snprintf(fullFilename, sizeof(fullFilename), "%s%s", filename, ext);
    else {
        strncpy(fullFilename, filename, sizeof(fullFilename));
        fullFilename[sizeof(fullFilename)-1] = '\0';
    }
    FILE *fp = fopen(fullFilename, "w");
    if (!fp) { perror("fopen"); return; }
    Line *curr = program;
    while (curr) {
        fprintf(fp, "%d %s\n", curr->number, curr->text);
        curr = curr->next;
    }
    fclose(fp);
}

void loadProgram(const char *filename) {
    const char *ext = ".pbcb";
    size_t len = strlen(filename);
    char fullFilename[MAX_LINE];
    if (len < strlen(ext) || strcmp(filename + len - strlen(ext), ext) != 0)
        snprintf(fullFilename, sizeof(fullFilename), "%s%s", filename, ext);
    else {
        strncpy(fullFilename, filename, sizeof(fullFilename));
        fullFilename[sizeof(fullFilename)-1] = '\0';
    }
    trimTrailingSpaces(fullFilename);
    FILE *fp = fopen(fullFilename, "r");
    if (!fp) { perror("fopen"); return; }
    newProgram();
    clearStringVars();
    char buffer[MAX_LINE];
    while (fgets(buffer, MAX_LINE, fp)) {
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        char *p = buffer;
        skipWhitespace(&p);
        if (p[0] == '\0') continue;
        if (isdigit(p[0])) {
            int num = atoi(p);
            while (isdigit(p[0])) p++;
            skipWhitespace(&p);
            if (p[0] != '\0')
                addProgramLine(num, p);
        }
    }
    fclose(fp);
}

/* Help Menu */
void show_help_menu(void) {
    printf("\033[?1049h");  // Enter alternate screen
    fflush(stdout);
    struct winsize ws;
    ioctl(0, TIOCGWINSZ, &ws);
    int width = ws.ws_col, height = ws.ws_row;
    const char *helpLines[] = {
        "----------------------------------------",
        "|            DotMis Help               |",
        "----------------------------------------",
        "| .r      - Run program                |",
        "| .ls     - List program lines         |",
        "| .new    - Clear program              |",
        "| .c      - Clear screen               |",
        "| .sav    - Save program               |",
        "| .loa    - Load program               |",
        "| .bep    - Beep                       |",
        "| .wt     - Wait (ms)                  |",
        "| //      - Comment                    |",
        "| .let    - Assignment                 |",
        "| .p      - Print                      |",
        "| .in     - Input                      |",
        "| .if .th - If..Then                   |",
        "| .gt     - Goto                       |",
        "| .gs     - Gosub                      |",
        "| .rtn    - Return                     |",
        "| .?      - Help                       |",
        "| .q      - Quit                       |",
        "| .e      - End program                |",
        "----------------------------------------",
        "  Press ESC or any key to exit help...  "
    };
    int nLines = sizeof(helpLines) / sizeof(helpLines[0]);
    int padTop = (height - nLines) / 2;
    for (int i = 0; i < padTop; i++) printf("\n");
    for (int i = 0; i < nLines; i++) {
        int len = strlen(helpLines[i]);
        int padLeft = (width - len) / 2;
        for (int j = 0; j < padLeft; j++) printf(" ");
        printf("%s\n", helpLines[i]);
    }
    fflush(stdout);
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\033[?1049l");  // Exit alternate screen
    fflush(stdout);
}

/* Statement Execution */
void executeStatement(char *s, int *nextLine, int *jumpLine) {
    skipWhitespace(&s);
    if (strncmp(s, "//", 2) == 0)
        return;
    else if (strncasecmp(s, ".let", 4) == 0) {
        s += 4; skipWhitespace(&s);
        if (!isalpha(s[0])) { error("[???] Expected variable after .let"); return; }
        char var = toupper(s[0]); s++;
        int isString = 0;
        if (s[0] == '$') { isString = 1; s++; }
        skipWhitespace(&s);
        if (s[0] != '=') { error("[???] Expected '=' in .let"); return; }
        s++; skipWhitespace(&s);
        if (isString) {
            if (s[0] != '"') { error("[???] Expected string literal in .let"); return; }
            s++;
            char buffer[MAX_LINE]; int i = 0;
            while (s[0] && s[0] != '"' && i < MAX_LINE - 1) { buffer[i++] = s[0]; s++; }
            buffer[i] = '\0';
            if (s[0] == '"') s++;
            if (strVars[var - 'A']) free(strVars[var - 'A']);
            strVars[var - 'A'] = strdup(buffer);
        } else {
            long double value = parseExpression(&s);
            numVars[var - 'A'] = value;
        }
    }
    else if (strncasecmp(s, ".p", 2) == 0) {
        s += 2;
        while (1) {
            skipWhitespace(&s);
            if (s[0] == '\0') break;
            if (s[0] == '"') {
                s++;
                char buffer[MAX_LINE]; int i = 0;
                while (s[0] && s[0] != '"' && i < MAX_LINE - 1) { buffer[i++] = s[0]; s++; }
                buffer[i] = '\0';
                if (s[0] == '"') s++;
                printf("%s", buffer);
            } else if (isalpha(s[0])) {
                char var = toupper(s[0]); s++;
                if (s[0] == '$') { s++;
                    if (strVars[var - 'A']) printf("%s", strVars[var - 'A']);
                    else printf("(null)");
                } else {
                    skipWhitespace(&s);
                    if (s[0] == '\0')
                        printf("%.0Lf", numVars[var - 'A']);
                    else {
                        long double value = parseExpression(&s);
                        printf("%.0Lf", value);
                    }
                }
            } else {
                long double value = parseExpression(&s);
                printf("%.0Lf", value);
            }
            skipWhitespace(&s);
            if (s[0] == ';') { s++; continue; }
            break;
        }
        printf("\n");
    }
    else if (strncasecmp(s, ".in", 3) == 0) {
        s += 3; skipWhitespace(&s);
        if (!isalpha(s[0])) { error("[???] Expected variable after .in"); return; }
        char var = toupper(s[0]); s++;
        int isString = 0;
        if (s[0] == '$') { isString = 1; s++; }
        char prompt[64];
        if (isString)
            snprintf(prompt, sizeof(prompt), "Input string for %c$: ", var);
        else
            snprintf(prompt, sizeof(prompt), "Input value for %c: ", var);
        char *input = readline(prompt);
        if (!input) return;
        if (isString) {
            if (strVars[var - 'A']) free(strVars[var - 'A']);
            strVars[var - 'A'] = strdup(input);
        } else {
            char *p = input;
            long double value = strtold(p, &p);
            numVars[var - 'A'] = value;
        }
        free(input);
    }
    else if (strncasecmp(s, ".if", 3) == 0) {
        s += 3; skipWhitespace(&s);
        long double left = parseExpression(&s);
        skipWhitespace(&s);
        char op[3] = {0};
        if (s[0] == '<' || s[0] == '>' || s[0] == '=') {
            op[0] = s[0]; s++;
            if (s[0] == '=' || (s[0] == '>' && op[0]=='<')) { op[1] = s[0]; s++; }
        } else { error("[!!!] Expected relational operator in .if"); return; }
        skipWhitespace(&s);
        long double right = parseExpression(&s);
        int cond = 0;
        if (strcmp(op, "=") == 0) cond = (left == right);
        else if (strcmp(op, "<") == 0) cond = (left < right);
        else if (strcmp(op, ">") == 0) cond = (left > right);
        else if (strcmp(op, "<=") == 0) cond = (left <= right);
        else if (strcmp(op, ">=") == 0) cond = (left >= right);
        else if (strcmp(op, "<>") == 0) cond = (left != right);
        else { error("[!!!] Unknown relational operator"); return; }
        skipWhitespace(&s);
        if (strncasecmp(s, ".th", 3) == 0) {
            s += 3; skipWhitespace(&s);
            // Accept any command after .th (no .gt requirement)
        } else { error("[!!!] Expected .th in .if"); return; }
        int lineNum = atoi(s);
        if (lineNum == 0) { error("[!!!] Expected line number after .th"); return; }
        if (cond) { *jumpLine = lineNum; *nextLine = 0; }
    }
    else if (strncasecmp(s, ".gt", 3) == 0) {
        s += 3; skipWhitespace(&s);
        int lineNum = atoi(s);
        if (lineNum == 0) { error("[!!!] Expected line number after .gt"); return; }
        *jumpLine = lineNum; *nextLine = 0;
    }
    else if (strncasecmp(s, ".gs", 3) == 0) {
        s += 3; skipWhitespace(&s);
        int lineNum = atoi(s);
        if (lineNum == 0) { error("[!!!] Expected line number after .gs"); return; }
        if (gosubStackTop < GOSUB_STACK_SIZE - 1) {
            Line *curr = program; int returnLine = 0;
            while (curr) { if (curr->number > lineNum) { returnLine = curr->number; break; } curr = curr->next; }
            gosubStack[++gosubStackTop] = returnLine;
        } else { error("[!!!] GOSUB stack overflow"); }
        *jumpLine = lineNum; *nextLine = 0;
    }
    else if (strncasecmp(s, ".rtn", 4) == 0) {
        if (gosubStackTop < 0) { error("[!!!] GOSUB stack underflow"); return; }
        *jumpLine = gosubStack[gosubStackTop--]; *nextLine = 0;
    }
    else if (strncasecmp(s, ".wt", 3) == 0) {
        s += 3; skipWhitespace(&s);
        int ms = atoi(s);
        if (ms > 0) usleep(ms * 1000);
    }
    else if (strncasecmp(s, ".bep", 4) == 0) {
        s += 4; skipWhitespace(&s);
        int freq = atoi(s);
        while (isdigit(s[0])) s++;
        skipWhitespace(&s);
        int dur = atoi(s);
        if (freq <= 0 || dur <= 0) error("[???] Invalid freq/dur for .bep");
        else beepCommand(freq, dur);
    }
    else if (strncasecmp(s, ".?", 2) == 0) {
        show_help_menu();
    }
    else if (strncasecmp(s, ".q", 2) == 0) {
        *nextLine = 0; exit(0);
    }
    else if (strncasecmp(s, ".e", 2) == 0) {
        *nextLine = 0;
    }
    else {
        error("?");
    }
}

/* Program Execution */
void runProgram(void) {
    memset(numVars, 0, sizeof(numVars));
    clearStringVars();
    gosubStackTop = -1;
    Line *current = program;
    interrupted = 0;
    while (current) {
        currentProgramLine = current->number;
        if (interrupted) { printf("\nBreak\n"); interrupted = 0; break; }
        char *s = current->text;
        int nextLine = 1, jumpLine = 0;
        executeStatement(s, &nextLine, &jumpLine);
        if (jumpLine != 0) {
            Line *target = findLine(jumpLine);
            if (!target) { error("Target line not found"); return; }
            current = target;
        } else if (nextLine) {
            current = getNextLine(current);
        } else break;
    }
    currentProgramLine = 0;
}

/* Main Loop */
int main(void) {
    signal(SIGINT, sigint_handler);
    printf("DotMis v1.0 by Plastic Bottleneck\n");
#ifdef __linux__
    struct sysinfo info;
    if (sysinfo(&info) == 0)
        printf("%lu MB free (OK)\n", info.freeram / (1024 * 1024));
    else
        printf("Free RAM unknown (OK)\n");
#else
    printf("Free RAM unknown (OK)\n");
#endif
    while (1) {
        char *input = readline("> ");
        if (!input) break;
        if (input[0] != '\0')
            add_history(input);
        char linebuf[MAX_LINE];
        strncpy(linebuf, input, MAX_LINE - 1);
        linebuf[MAX_LINE - 1] = '\0';
        free(input);
        char *p = linebuf;
        skipWhitespace(&p);
        if (p[0] == '\0') continue;
        if (isdigit(p[0])) {
            int num = atoi(p);
            while (isdigit(p[0])) p++;
            skipWhitespace(&p);
            if (p[0] == '\0')
                deleteProgramLine(num);
            else
                addProgramLine(num, p);
        } else {
            if (strncasecmp(p, ".r", 2) == 0)
                runProgram();
            else if (strncasecmp(p, ".ls", 3) == 0)
                listProgram();
            else if (strncasecmp(p, ".new", 4) == 0) { newProgram(); clearStringVars(); }
            else if (strncasecmp(p, ".c", 2) == 0)
                printf("\033[H\033[J");
            else if (strncasecmp(p, ".sav", 4) == 0) {
                p += 4; skipWhitespace(&p);
                if (p[0] == '\0')
                    error("Filename required for .sav");
                else
                    saveProgram(p);
            } else if (strncasecmp(p, ".loa", 4) == 0) {
                p += 4; skipWhitespace(&p);
                if (p[0] == '\0')
                    error("Filename required for .loa");
                else
                    loadProgram(p);
            } else if (strncasecmp(p, ".q", 2) == 0) {
                break;
            } else {
                int dummy = 1, jump = 0;
                executeStatement(p, &dummy, &jump);
            }
        }
    }
    newProgram();
    clearStringVars();
    return 0;
}
