#include "./terminal.h"
#include "./keyboard.h"
#include "../lib/stdio.h"
#include "../lib/cli.h"

static uint32_t terminal_state = 0;
int32_t terminal_open(){
    clear();
    return 0;
}
int32_t terminal_close(){
    clear();
    return 0;    
}
unsigned char keyboard_buffer_copy[128];
unsigned char buffer_end_copy;
static bool ready_for_reading = 0;
int32_t terminal_read(void* buf, int32_t nbytes){
    if(buf==NULL)
        return 0;
    while(!ready_for_reading);
    unsigned long flags;
    cli_and_save(flags);    
    uint32_t i;
    for(i=0;i<buffer_end_copy && i<nbytes;++i){
        *(((unsigned char*)buf)+i) = keyboard_buffer_copy[i];
    }
    ready_for_reading = 0;
    restore_flags(flags);
    return i;
}
int32_t terminal_write(void* buf, int32_t nbytes){
    if(buf==NULL)
        return 0;
    unsigned long flags;
    cli_and_save(flags); 
     uint32_t i;
    for(i=0; i<nbytes;++i){
        putc(*(((unsigned char*)buf)+i));
    }   
    restore_flags(flags);
    return i;
}
void terminal_update_keyboard(int32_t flag, unsigned char a){
    if(terminal_state==0){
        if(flag==-1){
            backspace();
        }
        else if (flag==1){
            putc(a);
            if(a == '\n'){
                unsigned char i;
                for(i=0;i<128;++i){
                    keyboard_buffer_copy[i] = keyboard_buffer[i];
                }
                buffer_end_copy = buffer_end;
                ready_for_reading = 1;
                keyboard_buffer_clear();
            }
            
        }
    }
}