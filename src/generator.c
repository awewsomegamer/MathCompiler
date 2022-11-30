#include <generator.h>
#include <lexer.h>
#include <string.h>

uint8_t* buffer = NULL;
uint8_t* data_buffer = NULL;

int position = 0;
int data_position = 0;

#define append_byte(b) \
        buffer = realloc(buffer, ++position); \
        *(buffer + position - 1) = b;

int reg = 0;
int xmm_reg = 0;

struct reference {
        double value;
        uint64_t position;
};

struct reference* references;
int reference_count = 0;

void insert_reference(double value, uint64_t position) {
        references = realloc(references, sizeof(struct reference) * (++reference_count));

        references[reference_count - 1].position = position;
        references[reference_count - 1].value = value;
}

int evaluate(tree_code_t *tree) {
        int left = 0, right = 0;

        if (tree->left != NULL) left = evaluate(tree->left);
        if (tree->right != NULL) right = evaluate(tree->right);

        switch (tree->type) {
        case T_INT:
                append_byte(0x48);
                append_byte(0xC7);
                append_byte((0xC0 + reg));
                
                // if mov reg, 0, use xor reg, reg
                // if mov rax, imm use mov rax op
                for (int i = 0; i < 4; i++) {
                        int value = (int)tree->value;
                        append_byte(((value >> (8 * i)) & 0xFF));
                }
                
                append_byte(0xF2);
                append_byte(0x48 + ((xmm_reg >= 8) * 4));
                append_byte(0x0F);
                append_byte(0x2A);
                append_byte(0xC0 + (xmm_reg * 8));
                xmm_reg++;

                return xmm_reg - 1;

        case T_NUMBER:
                append_byte(0xF2);
                append_byte(0x0F);
                append_byte(0x10);
                append_byte(0x04 | ((int)(xmm_reg / 2) << 4) + (xmm_reg % 3) * 8);
                append_byte(0x0D);
                insert_reference(tree->value, position);
                for(int i = 0; i < 4; i++) append_byte(0x00);

                xmm_reg++;

                return xmm_reg - 1;

        case T_ADD:
                append_byte(0xF2);
                append_byte(0x0F);
                append_byte(0x58);
                append_byte(0xC0 + right + (left * 8));  


                return left;
        }

        return 0;
}

void fill_references() {
        struct reference* present_references = malloc(sizeof(struct reference));
        int present_reference_count = 0;

        for (int i = 0; i < reference_count; i++) {
                int present_reference_idx = -1;
                for (int j = 0; j < present_reference_count; j++) {
                        if (present_references[j].value == references[i].value) {
                                goto FILL_REFERENCE;
                        }
                }

                present_references = realloc(present_references, sizeof(struct reference) * (++present_reference_count));
                present_references[present_reference_count - 1].position = data_position;
                present_references[present_reference_count - 1].value = references[i].value;

                uint64_t bytes_double;
                memcpy(&bytes_double, &references[i].value, sizeof(uint64_t));
                printf("%02X\n", bytes_double);

                for (int j = 0; j < 8; j++) {
                        data_buffer = realloc(data_buffer, ++data_position);
                        *(data_buffer + data_position - 1) = (bytes_double >> (8 * j)) & 0xFF;
                }

                present_reference_idx = present_reference_count - 1;
                
                FILL_REFERENCE:

                printf("%X B\n", &buffer);

                for (int j = 0; j < 4; j++)
                        *(buffer + references[i].position + j) = ((present_references[present_reference_idx].position) >> (8 * j)) & 0xFF;
        }
}

code_block_t default_x86_64_generator(tree_code_t *tree) {
        code_block_t* ret = malloc(sizeof(code_block_t));
        buffer = malloc(1);
        data_buffer = malloc(1);
        position = 0;
        data_position = 0;

        // PUSH RBP
        append_byte(0x55);

        // MOV RBP, RSP
        append_byte(0x48);
        append_byte(0x89);
        append_byte(0xE5);

        // MOV RCX, DATA_SECTION
        append_byte(0x48);
        append_byte(0xB9);
        append_byte(0xBB);
        for (int i = 0; i < 7; i++) append_byte(0x00);
        
        evaluate(tree);

        // POP RBP
        append_byte(0x5D);

        // RET
        append_byte(0xC3);

        // Fill data buffer and get present reference list
        struct reference* present_references = malloc(sizeof(struct reference));
        int present_reference_count = 0;

        for (int i = 0; i < reference_count; i++) {
                int present_reference_idx = -1;
                for (int j = 0; j < present_reference_count; j++)
                        if (present_references[j].value == references[i].value)
                                continue;

                present_references = realloc(present_references, sizeof(struct reference) * (++present_reference_count));
                present_references[present_reference_count - 1].position = data_position;
                present_references[present_reference_count - 1].value = references[i].value;

                uint64_t bytes_double;
                memcpy(&bytes_double, &references[i].value, sizeof(uint64_t));

                for (int j = 0; j < 8; j++) {
                        data_buffer = realloc(data_buffer, ++data_position);
                        *(data_buffer + data_position - 1) = (bytes_double >> (8 * j)) & 0xFF;
                }
        }

        // Fill in code buffer and add data buffer (if there is one to code buffer)
        if (data_position > 0) {
                int pos_pre_add = position;
                buffer = realloc(buffer, position + data_position);
                for (int i = position; i < position + data_position; i++)
                        *(buffer + i) = *(data_buffer + (i - position));    

                for (int i = 0; i < reference_count; i++) {
                        for (int j = 0; j < present_reference_count; j++) {
                                if (present_references[j].value == references[i].value) {
                                        for (int x = 0; x < 4; x++)
                                                *(buffer + references[i].position + j) = ((present_references[j].position) >> (8 * x)) & 0xFF;
                         
                                        break;
                                }
                        }
                }

                for (int i = 0; i < 7; i++)
                        *(buffer + 7 + i) = ((((uintptr_t)&buffer) + position) >> (8 * i)) & 0xFF;
        }


        for (int i = 0; i < position + data_position; i++)
                printf("%02X ", *(buffer + i));
        printf("\n");

        ret->func = buffer;
        ret->size = position;

        free(data_buffer);

        return *ret;
}