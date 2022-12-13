import serial
import pygame
from pygame.locals import *
import math
 
# Define constants for colors
WHITE = (250, 250, 250)
DARKBLUE = (36, 90, 190)
LIGHTBLUE = (0, 176, 240)
RED = (207, 102, 121)
DARK_RED = (250,0,0)
ORANGE = (255, 100, 0)
YELLOW = (255, 255, 0)
BLACK = (18, 18, 18)
GREEN = (0, 255, 0)
TEAL = (3, 218, 198)
GREY = (150, 150, 150)
DARKGREY = (46, 46, 46)
GREY900 = (34, 34, 34)
GREY800 = (66, 66, 66)
GREY700 = (97, 97, 97)
GREY600 = (117, 117, 117)
 
# Open a new window for the game
HEIGHT = 800
WIDTH = 800
 
midx, midy = WIDTH/2, HEIGHT/2
quit = False
moving = False
dist_locked = False
dist_locked_num = -1
 
size = (WIDTH, HEIGHT)
num_textboxes = 5
textbox_width = WIDTH / num_textboxes - 40
screen = pygame.display.set_mode(size)
 
xdist, ydist = 0, 100
angle, distance = 0, 100
volume = 10
fleft, fright = 5000, 5000
filt = 1
 
# Volume slider
 
# slider = Slider(screen, 100, 100, 800, 40, min=0, max=99, step=1)
# output = TextBox(screen, 475, 200, 50, 50, fontSize=30)
# output.disable()  # Act as label instead of textbox
 
# Initialize user text input
 
# basic font for user typed
x_text = ''
y_text = ''
angle_text = ''
dist_text = ''
fleft_text = ''
fright_text = ''
 
# create rectangles
main_rect = pygame.Rect(0, 36, WIDTH, HEIGHT-72)
x_in_rect = pygame.Rect(40, 0, textbox_width, 36)
y_in_rect = pygame.Rect((WIDTH/num_textboxes) + 40, 0, textbox_width, 36)
angle_in_rect = pygame.Rect(2*(WIDTH/num_textboxes) + 40, 0, textbox_width, 36)
dist_in_rect = pygame.Rect(3*(WIDTH/num_textboxes) + 40, 0, textbox_width, 36)
fleft_in_rect = pygame.Rect(40, HEIGHT - 36, textbox_width, 36)
fright_in_rect = pygame.Rect((WIDTH/num_textboxes) + 40, HEIGHT - 36, textbox_width, 36)
 
# color_active stores color which
# gets active when input box is clicked by user
color_active = GREY700
 
# color_passive store color which is
# color of input box.
color_passive = GREY800
color = color_passive
 
x_in_active = False
y_in_active = False
angle_in_active = False
dist_in_active = False
fleft_in_active = False
fright_in_active = False
 
# Draw circle for audio source
source = pygame.draw.circle(screen, TEAL, [midx + xdist, midy - ydist], 10, 0)
 
# Draw circle for person
person = pygame.draw.circle(screen, RED, [midx, midy], 20, 0)


filter_toggle = pygame.draw.circle(screen, GREEN, [WIDTH-18, HEIGHT-36+18], 10, 0)
 
throttle = 0
 
# Initialize the clock, which controls the rate at which our screen updates
clock = pygame.time.Clock()
 
def init():
    # Initialize pygame
    pygame.init()
    pygame.event.set_blocked(None)
    pygame.event.set_allowed([QUIT, MOUSEBUTTONDOWN, MOUSEBUTTONUP, MOUSEMOTION, KEYDOWN])
    pygame.event.clear()
    pygame.display.set_caption("Spatial Audio")
   
 
def parseInput(ser):
    some_bytes = ser.readline()
    decoded_bytes = some_bytes[0:len(some_bytes)].decode("utf-8")
    return decoded_bytes[0]
 

def getAngle(x, y): # -180 to 180, with 0 being x=0, y>0
    rad = math.atan2(-x, y)
    return int(rad*180/math.pi)
 
def drawUpdate():
    global x_text, y_text, angle_text, dist_text, fleft_text, fright_text
    global x_in_rect, y_in_rect, angle_in_rect, dist_in_rect, fleft_in_rect, fright_in_rect
    global filt
 
    screen.fill(DARKGREY)
 
    pygame.draw.circle(screen, GREY, [midx, midy], 100, 2)
    pygame.draw.circle(screen, GREY, [midx, midy], 200, 2)
    pygame.draw.circle(screen, GREY, [midx, midy], 300, 2)
    pygame.draw.circle(screen, GREY, [midx, midy], 400, 2)
    pygame.draw.circle(screen, GREY, [midx, midy], 500, 2)
 
    # pygame.draw.line(screen, WHITE, [0, 38], [800, 38], 2)
 
    # Person and Source
    pygame.draw.line(screen, WHITE, [person.centerx, person.centery], [source.centerx, source.centery], 2)
    pygame.draw.circle(screen, RED, [midx, midy], 20, 0)
    pygame.draw.circle(screen, TEAL, source.center, 10, 0)
 
    # Boxes
    pygame.draw.rect(screen, DARKGREY, [0, 0, WIDTH, 36], 0)
    pygame.draw.line(screen, WHITE, [0, 36], [WIDTH, 36], 2)
 
    pygame.draw.rect(screen, DARKGREY, [0, HEIGHT - 36, WIDTH, 36], 0)
    pygame.draw.line(screen, WHITE, [0, HEIGHT - 38], [WIDTH, HEIGHT-38], 2)
 
 
    # Text
    font = pygame.font.Font(None, 28)
    textx = font.render(f"x: ", 1, WHITE)
    texty = font.render(f"y: ", 1, WHITE)
    textangle = font.render(f"a: ", 1, WHITE)
    textdist = font.render(f"d: ", 1, WHITE)
    textvol = font.render(f"v: {volume}", 1, WHITE)
    texton = font.render(f"FILTER ON: ", 1, WHITE)
    textr = font.render(f"R", 1, WHITE)
    textl = font.render(f"L", 1, WHITE)
    textfleft = font.render(f"fl: ", 1, WHITE)
    textfright = font.render(f"fr: ", 1, WHITE)
    screen.blit(textx, (10, 10))
    screen.blit(texty, ((WIDTH/num_textboxes) + 10, 10))
    screen.blit(textangle, (2*(WIDTH/num_textboxes) + 10, 10))
    screen.blit(textdist, (3*(WIDTH/num_textboxes) + 10, 10))
    screen.blit(textvol, (4*(WIDTH/num_textboxes) + 10, 10))
    screen.blit(texton, (WIDTH - 150, HEIGHT-36+10))
    screen.blit(textl, (10, HEIGHT/2 - 16))
    screen.blit(textr, (WIDTH - 25, HEIGHT/2 - 16))
    # screen.blit(textfleft, (10, HEIGHT-36+10))
    # screen.blit(textfright, ((WIDTH/num_textboxes) + 10, HEIGHT-36+10))
 
    # Draw user input boxes
 
    x_in_color, y_in_color, angle_in_color, dist_in_color = (color_passive, color_passive, color_passive, color_passive)
    fleft_in_color, fright_in_color = (color_passive, color_passive)
 
    if x_in_active:
        x_in_color = color_active
    else:
        x_text = str(xdist)
 
    if y_in_active:
        y_in_color = color_active
    else:
        y_text = str(ydist)
 
    if angle_in_active:
        angle_in_color = color_active
    else:
        angle_text = str(angle)
 
    if dist_in_active:
        dist_in_color = color_active
    else:
        dist_text = str(distance)
 
    if fleft_in_active:
        fleft_in_color = color_active
    else:
        fleft_text = str(fleft)
 
    if fright_in_active:
        fright_in_color = color_active
    else:
        fright_text = str(fright)
 
 
       
    # draw rectangle and argument passed which should
    # be on screen
    pygame.draw.rect(screen, x_in_color, x_in_rect)
    pygame.draw.rect(screen, y_in_color, y_in_rect)
    pygame.draw.rect(screen, angle_in_color, angle_in_rect)
    pygame.draw.rect(screen, dist_in_color, dist_in_rect)
    # pygame.draw.rect(screen, fleft_in_color, fleft_in_rect)
    # pygame.draw.rect(screen, fright_in_color, fright_in_rect)
 
    x_text_surface = font.render(x_text, 1, WHITE)
    y_text_surface = font.render(y_text, 1, WHITE)
    angle_text_surface = font.render(angle_text, 1, WHITE)
    dist_text_surface = font.render(dist_text, 1, WHITE)
    fleft_text_surface = font.render(fleft_text, 1, WHITE)
    fright_text_surface = font.render(fright_text, 1, WHITE)
   
    # render at position stated in arguments
    screen.blit(x_text_surface, (x_in_rect.x+10, x_in_rect.y+10))
    screen.blit(y_text_surface, (y_in_rect.x+10, y_in_rect.y+10))
    screen.blit(angle_text_surface, (angle_in_rect.x+10, angle_in_rect.y+10))
    screen.blit(dist_text_surface, (dist_in_rect.x+10, dist_in_rect.y+10))
    # screen.blit(fleft_text_surface, (fleft_in_rect.x+10, fleft_in_rect.y+10))
    # screen.blit(fright_text_surface, (fright_in_rect.x+10, fright_in_rect.y+10))

    if filt==1:
        pygame.draw.circle(screen, GREEN, filter_toggle.center, 10, 0)
    else:
        pygame.draw.circle(screen, DARK_RED, filter_toggle.center, 10, 0)
 
   
    # set width of textfield so that text cannot get
    # outside of user's text input
    #input_rect.w = max(100, text_surface.get_width()+10)
 
    # output.setText(slider.getValue())
 
    # pygame_widgets.update(event)
 
    # Update screen after drawing
    pygame.display.flip()
 
 
def handleEvent(event):
    global moving, dist_locked, dist_locked_num
    global filt
    global x_in_active, y_in_active, angle_in_active, dist_in_active, fleft_in_active, fright_in_active
    global x_text, y_text, angle_text, dist_text, fleft_text, fright_text
    global volume, distance, fleft, fright
 
    if event.type == MOUSEBUTTONDOWN:
        if event.button == 1:
            x_in_active, y_in_active, angle_in_active, dist_in_active = False, False, False, False
            fleft_in_active, fright_in_active = False, False
            if source.collidepoint(event.pos):
                moving = True
            elif main_rect.collidepoint(event.pos):
                pos_x, pos_y = event.pos
                rel = (pos_x - source.centerx, pos_y - source.centery)
                source.move_ip(rel)
            elif filter_toggle.collidepoint(event.pos):
                filt = 1-filt
            elif x_in_rect.collidepoint(event.pos):
                x_in_active = True
            elif y_in_rect.collidepoint(event.pos):
                y_in_active = True
            elif angle_in_rect.collidepoint(event.pos):
                angle_in_active = True
            elif dist_in_rect.collidepoint(event.pos):
                dist_in_active = True
            elif fleft_in_rect.collidepoint(event.pos):
                fleft_in_active = True
            elif fright_in_rect.collidepoint(event.pos):
                fright_in_active = True
 
        elif event.button == 4:
            #SCROLL UP, ZOOM IN
            pass
        elif event.button == 5:
            #SCROLL DOWN, ZOOM OUT
            pass
 
    elif event.type == MOUSEBUTTONUP:    
        moving = False
 
    elif event.type == MOUSEMOTION:
        if moving and dist_locked:
            pos_x, pos_y = event.pos
            mouse_xdist = pos_x - person.centerx
            mouse_ydist = person.centery - pos_y
            if mouse_xdist == 0: mouse_angle = math.pi / 2
            else: mouse_angle = math.atan(mouse_ydist / mouse_xdist)
            if mouse_xdist < 0: mouse_angle = mouse_angle + math.pi
 
            new_pos_x = int(dist_locked_num * math.cos(mouse_angle)) + person.centerx
            new_pos_y = person.centery - int(dist_locked_num * math.sin(mouse_angle))
 
            rel = (new_pos_x - source.centerx, new_pos_y - source.centery)
            source.move_ip(rel)
 
        elif moving and main_rect.collidepoint(event.pos): source.move_ip(event.rel)

        elif moving: moving = False
 
    elif event.type == pygame.KEYDOWN:
 
        # Volume control
 
        if event.key == pygame.K_DOWN and volume >= 1:
            volume -= 1
        elif event.key == pygame.K_UP and volume <= 99:
            volume += 1
 
        # Distance Locking
 
        if event.key == pygame.K_LSHIFT:
            dist_locked = not dist_locked
            dist_locked_num = distance
 
        # Text box input
 
        user_in_text = ''
        if x_in_active: user_in_text = x_text
        elif y_in_active: user_in_text = y_text
        elif angle_in_active: user_in_text = angle_text
        elif dist_in_active: user_in_text = dist_text
        elif fleft_in_active: user_in_text = fleft_text
        elif fright_in_active: user_in_text = fright_text
 
        # Check for backspace
        if event.key == pygame.K_BACKSPACE:
 
            # get text input from 0 to -1 i.e. end.
            user_in_text = user_in_text[:-1]
 
        # Unicode standard is used for string
        # formation
        else:
            if len(user_in_text) < 5:
                user_in_text += event.unicode
 
        value_entered = (event.unicode == '\n' or event.unicode == '\r')
 
        if x_in_active:
            x_text = user_in_text
            if value_entered:
                x_text = user_in_text[:-1]
                offset = 0
                try: offset = person.centerx + int(x_text) - source.centerx
                except: offset = 0
                source.move_ip(offset, 0)
   
        elif y_in_active:
            y_text = user_in_text
            if value_entered:
                y_text = user_in_text[:-1]
                offset = 0
                try: offset = person.centery - int(y_text) - source.centery
                except: offset = 0
                source.move_ip(0, offset)
 
        elif angle_in_active:
            angle_text = user_in_text
            if value_entered:
                angle_text = user_in_text[:-1]
                offset = (0, 0)
                try: # change angle with same distance
                    new_angle = int(angle_text) / 360 * 2 * math.pi
                    new_pos_x = int(distance * math.cos(new_angle)) + person.centerx
                    new_pos_y = person.centery - int(distance * math.sin(new_angle))
                    offset = (new_pos_x - source.centerx, new_pos_y - source.centery)
 
                except: offset = (0, 0)
                source.move_ip(offset)
 
        elif dist_in_active:
            dist_text = user_in_text
            if value_entered:
                dist_text = user_in_text[:-1]
                offset = (0, 0)
                try: # change distance with same angle
                    new_dist = int(dist_text)
                    angle_rad = angle / 360 * 2 * math.pi
                    new_pos_x = int(new_dist * math.cos(angle_rad)) + person.centerx
                    new_pos_y = person.centery - int(new_dist * math.sin(angle_rad))
                    offset = (new_pos_x - source.centerx, new_pos_y - source.centery)
 
                except: offset = (0, 0)
                source.move_ip(offset)
               
 
        elif fleft_in_active:
            fleft_text = user_in_text
            if value_entered:
                fleft_text = user_in_text[:-1]
                fleft = int(fleft_text)
 
        elif fright_in_active:
            fright_text = user_in_text
            if value_entered:
                fright_text = user_in_text[:-1]
                fright = int(fright_text)
 
    else: print(event)
 
    # Keeping the source in bounds
 
    if source.centerx > WIDTH:
        source.move_ip(WIDTH - source.centerx, 0)
    elif source.centerx < 0:
        source.move_ip(-source.centerx, 0)
    if source.centery > WIDTH:
        source.move_ip(0, HEIGHT - source.centery)
    elif source.centery < 36:
        source.move_ip(0, 36-source.centery)
 
 
##############################################################################
init()
 
# Main game loop:
with serial.Serial('COM3',115200) as ser:
    while not quit:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                quit = True
                break
            else:
                handleEvent(event)
 
        throttle += 1
        if throttle >= 5:
            xdist = source.centerx - person.centerx
            ydist = person.centery - source.centery # so up is positive y

            angle = getAngle(xdist, ydist)
            distance = int(math.sqrt(xdist**2 + ydist**2))
            dist_l = int(math.sqrt((xdist+10)**2 + ydist**2))
            dist_r = int(math.sqrt((xdist-10)**2 + ydist**2))
 
            fleft_scaled = int(fleft / 5000 * 1024)
            fright_scaled = int(fright / 5000 * 1024)

            angle_l = math.atan2(-(xdist+10), ydist)
            angle_r = math.atan2(-(xdist-10), ydist)

            left_amp = int(250.0*(3.0+math.cos(angle_l - (math.pi/4))))
            right_amp = int(250.0*(3.0+math.cos(angle_r + (math.pi/4))))

            left_amp = int(left_amp * min(1.0, 130.0/(100.0+dist_l)))
            right_amp = int(right_amp * min(1.0, 130.0/(100.0+dist_l)))
 
            ser.flush()
            ser.write(bytes(f"x{xdist}y{ydist}v{volume}a{angle}f{filt}l{left_amp}r{right_amp}\r", 'utf-8'))
            #ser.write(bytes(f"x{xdist}y{ydist}v{volume}a{angle}f{filt}l{fleft_scaled}r{fright_scaled}\r", 'utf-8'))
            #print(f"x: {xdist}, y: {ydist}, v: {volume}, a: {angle}, f: {filt}, l: {fleft}, r: {fright}")
            throttle = 0
 
            # userInput = parseInput(ser)
            # print(userInput)
 
        drawUpdate()
 
        clock.tick(60) # FPS
 
# Once we have exited the main program loop we can stop the game engine:
pygame.quit()
 

