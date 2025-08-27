        // Version debug intensifiée de la boucle principale
        for(;;){
            // DEBUG: Avant l'appel syscall
            putc('[');
            putc('G');
            putc('E');
            putc('T');
            putc(']');
            
            int c = sys_getchar();
            
            // DEBUG: Après l'appel syscall
            putc('[');
            putc('R');
            putc('E');
            putc('T');
            putc(':');
            if (c >= 32 && c <= 126) {
                putc((char)c);
            } else {
                putc('?');
            }
            putc(']');
            
            if (c == '\r' || c == '\n'){
                putc('\n');
                handle_line(ctx, buf);
                break;
            }
            if (c == 0x08 || c == 127){ // Backspace
                if (idx > 0){
                    idx--;
                    buf[idx] = '\0';
                    backspace();
                }
                continue;
            }
            if (idx < (int)sizeof(buf) - 1){
                buf[idx++] = (char)c;
                buf[idx] = '\0';
                putc((char)c);
            }
        }
