from typing import Optional
import pygame
from src.liveviewer import LiveViewer
import serial.tools.list_ports as ports


def create_port_menu() -> Optional[str]:
    """
    Create a Pygame-based menu for serial port selection.
    
    Returns:
        Optional[str]: Selected port device path or None if selection was cancelled
    """
    pygame.init()
    
    # Calculate required window size based on content
    com_ports = list(ports.comports())
    max_port_length = max(len(str(port.device)) for port in com_ports) if com_ports else 20
    window_width = max(600, max_port_length * 15)  # Ensure minimum width of 600px
    window_height = 100 + len(com_ports) * 40  # Height based on number of ports
    
    screen = pygame.display.set_mode((window_width, window_height))
    pygame.display.set_caption("Serial Port Selection")
    
    # Setup fonts and colors
    font = pygame.font.Font(None, 36)
    text_color = (220, 220, 220)
    highlight_color = (100, 150, 255)
    bg_color = (30, 30, 30)
    selected_bg_color = (50, 50, 50)
    
    if not com_ports:
        pygame.quit()
        raise RuntimeError("No serial ports found!")
    
    selected_index = 0
    clock = pygame.time.Clock()
    
    while True:
        screen.fill(bg_color)
        
        # Draw title with more standard arrow notation
        title = font.render("Select Serial Port (Use Up/Down, Enter to select)", True, text_color)
        screen.blit(title, (20, 20))
        
        # Draw port options
        for idx, port in enumerate(com_ports):
            # Draw selection background
            if idx == selected_index:
                pygame.draw.rect(screen, selected_bg_color, 
                               (15, 65 + idx * 40, window_width - 30, 35))
            
            # Draw port information
            port_text = f"{port.device} - {port.description}"
            color = highlight_color if idx == selected_index else text_color
            text = font.render(port_text, True, color)
            screen.blit(text, (20, 70 + idx * 40))
        
        pygame.display.flip()
        clock.tick(60)  # Limit to 60 FPS
        
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.display.quit()
                return None
                
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_UP:
                    selected_index = (selected_index - 1) % len(com_ports)
                elif event.key == pygame.K_DOWN:
                    selected_index = (selected_index + 1) % len(com_ports)
                elif event.key == pygame.K_RETURN:
                    selected_port = com_ports[selected_index].device
                    pygame.display.quit()
                    return selected_port


if __name__ == "__main__":
    selected_port = create_port_menu()
    if selected_port:
        LiveViewer(selected_port).run()
