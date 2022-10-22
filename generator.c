#include <stdio.h>
#include <immintrin.h>
#include <inttypes.h>

typedef struct {
    int sudoku[9 * 9];
} sudoku_t;

uint32_t random_rdrand32() {
    uint32_t result;
    _rdrand32_step(&result);
    return result;
}

void print_sudoku(sudoku_t *sudoku) {
    printf("=====================\n");
    for (int column = 0; column < 9; column++) {
        for (int row = 0; row < 9; row++) {
            printf("%d ", sudoku->sudoku[column * 9 + row]);
            if (row == 2 || row == 5) printf("| ");
        }
        printf("\n");
        if (column == 2 || column == 5) printf("---------------------\n");
    }
    printf("=====================\n");
}

int verify_sudoku(sudoku_t *sudoku) {
    int row_found = 0, column_found = 0;
    for (int i = 0; i < 9; i++) {
        for (int k = 0; k < 9; k++) {
            row_found |= 1 << sudoku->sudoku[i * 9 + k];
            column_found |= 1 << sudoku->sudoku[k * 9 + i];
        }
        if (row_found != 0b1111111110 || column_found != 0b1111111110) return 0;
        row_found = 0;
        column_found = 0;
    }

    int block_found = 0;
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            for (int i = 0; i < 3; i++) {
                for (int k = 0; k < 3; k++) {
                    block_found |= 1 << sudoku->sudoku[(x * 3 + i) * 9 + y * 3 + k];
                }
            }
            if (block_found != 0b1111111110) return 0;
            block_found = 0;
        }
    }

    return 1;
}


#define CHECK_ROW (1 << 0)
#define CHECK_COLUMN (1 << 1)
#define CHECK_BLOCKS (1 << 2)
int is_valid(sudoku_t *sudoku, int row, int column, int checks) {
    if (checks & CHECK_ROW) { // Check row
        int found[10] = {0};
        for (int row_pos = 0; row_pos < 9; row_pos++) {
            int number = sudoku->sudoku[row * 9 + row_pos];
            if (number == 0) continue;

            if (found[number]) { // Number already found before
                return 0;
            }
            found[number] = 1;
        }
    }

    if (checks & CHECK_COLUMN) { // Check column
        int found[10] = {0};
        for (int column_pos = 0; column_pos < 9; column_pos++) {
            int number = sudoku->sudoku[column_pos * 9 + column];
            if (number == 0) continue;

            if (found[number]) {
                return 0;
            }
            found[number] = 1;
        }
    }

    if (checks & CHECK_BLOCKS) { // Check block
        int row_start = (row / 3) * 3;
        int column_start = (column / 3) * 3;
        int found[10] = {0};
        for (int i = 0; i < 3; i++) {
            for (int k = 0; k < 3; k++) {
                int number = sudoku->sudoku[(row_start + i) * 9 + column_start + k];
                if (number == 0) continue;
                if (found[number]) return 0;
                found[number] = 1;
            }
        }
    }

    return 1;
}

void get_possible_numbers(sudoku_t *sudoku, int row, int column, int *possible) {
    int possible_bitwise = 0b111111111;

    for (int i = 0; i < 9; i++) {
        int row_number = sudoku->sudoku[row * 9 + i];
        int column_number = sudoku->sudoku[i * 9 + column];

        if (row_number >= 1 && column_number <= 9) possible_bitwise &= ~(1 << (row_number - 1));
        if (column_number >= 1 && column_number <= 9) possible_bitwise &= ~(1 << (column_number - 1));
    }

    int row_start = (row / 3) * 3;
    int column_start = (column / 3) * 3;

    for (int i = 0; i < 3; i++) {
        for (int k = 0; k < 3; k++) {
            int number = sudoku->sudoku[(row_start + i) * 9 + column_start + k];
            if (number >= 1 && number <= 9) possible_bitwise &= ~(1 << (number - 1));
        }
    }

    int amount = 0;
    for (int i = 0; i < 9; i++) {
        if ((possible_bitwise >> i) & 1) {
            ++amount;
            possible[amount] = i + 1;
        }
    }

    possible[0] = amount;
}

int generate_sudoku_row_swap_delete(sudoku_t *sudoku) {
    int error = 0;

    int failed_once = 0;
    int failed_twice = 0;

    int i = 0;
    while (i < 81) {
        int row = i / 9;
        int column = i % 9;

        int possible[10];
        get_possible_numbers(sudoku, row, column, possible);

        int amount_possible = possible[0];

        if (amount_possible < 1) { // Look for numbers to swap with current position
            int swap_success = 0;

            for (int row_pos = column - 1; row_pos >= 0; row_pos--) { // Iterate backwards through current row
                int row_number = sudoku->sudoku[row * 9 + row_pos];
                sudoku->sudoku[row * 9 + column] = row_number;
                sudoku->sudoku[row * 9 + row_pos] = 0;
                if (!is_valid(sudoku, row, column, CHECK_COLUMN | CHECK_BLOCKS)) { // Check if column and block will remain valid. If not, restore previous numbers
                    sudoku->sudoku[row * 9 + column] = 0;
                    sudoku->sudoku[row * 9 + row_pos] = row_number;
                    continue;
                }

                int new_possible[10];
                get_possible_numbers(sudoku, row, row_pos, new_possible);
                int new_amount_possible = new_possible[0];
                if (new_amount_possible < 1) { // Invalid swap: Set values back to what they were before and continue with next loop
                    sudoku->sudoku[row * 9 + column] = 0;
                    sudoku->sudoku[row * 9 + row_pos] = row_number;
                } else { // Set new value for swapped number
                    sudoku->sudoku[row * 9 + row_pos] = new_possible[1 + (random_rdrand32() % new_amount_possible)];
                    swap_success = 1;
                    break;
                }
            }

            if (!swap_success) { // No swaps were possible. Delete row(s) and retry
                if (failed_once) {
                    failed_twice = 1;
                    for (int row_pos = column; row_pos >= 0; row_pos--) {
                        sudoku->sudoku[row * 9 + row_pos] = 0;
                    }
                    for (int row_pos = 8; row_pos >= 0; row_pos--) {
                        sudoku->sudoku[(row - 1) * 9 + row_pos] = 0;
                    }
                    i = (row - 1) * 9;
                    continue;
                } else if (failed_twice) {
                    error = 1;
                    break;
                }

                failed_once = 1;
                for (int row_pos = column; row_pos >= 0; row_pos--) {
                    sudoku->sudoku[row * 9 + row_pos] = 0;
                }

                i = row * 9;
                continue;
            } else {
                ++i;
                continue;
            }
        }

        int number = possible[1 + (random_rdrand32() % amount_possible)];
        sudoku->sudoku[row * 9 + column] = number;

        ++i;
    }

    return error;
}

int main() {
    sudoku_t sudoku = {0};
    if (0 != generate_sudoku_row_swap_delete(&sudoku) && verify_sudoku(&sudoku)) {
        printf("Sudoku generation failed!\n");
        return EXIT_FAILURE;
    }

    print_sudoku(&sudoku);

    return EXIT_SUCCESS;
}
