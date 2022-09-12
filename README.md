# Disco : A freeform electronic desk toy

Disco is a advanced desktop "toy" which can be controlled via a Discord bot.
It consists of a 16x2 LCD, a single pushbutton and an RGB LED all controlled by a NodeMCU ESP32.

<p>
  <img src="readme%20images/thumb3.jpg" width="33%" />
  <img src="readme%20images/thumb1.jpg" width="33%" /> 
  <img src="readme%20images/thumb2.jpg" width="33%" />
</p>

## Schematic
![Schematic Disco](readme%20images/schematic.png)

Parts needed:
- NodeMCU ESP32
- LCD 16x2 (5V logic)
- RGB LED Common Cathode
- Pushbutton
- Potentiometer
- 1k Resistor
- 560Ω Resistor
- 220Ω Resistor
- 2x 150Ω Resistor

## Discord bot commands

### Set mode
![Setmode Discord bot command](readme%20images/setmode.png)

Setmode lets you set the main mode/functionality of Disco.
The options are:

|Mode|Example Screenshot|Description|
|-|-|-|
|`Clock & Temp mode`|![LCD screenshot of clock & temp mode](readme%20images/clockAndTemp.png)| Default mode. Show date, time and temperature|
|`Note mode`|![LCD screenshot of note mode](readme%20images/note.png)| Simple mode which displays the last note set with the `setnote` command|
|`Stocks mode`|![LCD screenshot of stocks mode](readme%20images/stocks.png)| This mode shows you the stock price and the growth percentage compared to the previous day. The stock displayed is set using the `setstock` command. The numbers are updated every 30 seconds|
|`Timer & Countdown mode`|![LCD screenshot of timer & countdown mode](readme%20images/countdown.png)| When you're in this mode you can set a timer with the `settimer` command or a countdown with the `setcountdown` command. Pressing the pushbutton will start/reset the clock. If a countdown reaches 0 the LED will flash and display a notification until dismissed|
|`Animation mode`|![LCD moving GIF of animation mode](readme%20images/animation.gif)| Shows a looping animation of frames set with the `setanimation` command. Up to 24 frames are possible and custom characters can be used|
|`3D viewer mode`|![LCD moving GIF of rotating cube in 3D viewer mode](readme%20images/3Dcube.gif)| This mode shows a rotating 3D object using the center 8 character positions. A custom 3D model can be set using the `setobject` command. Default is a rotating cube|


### Set Led Mode
![Set Led Mode Discord bot command](readme%20images/setLedMode.png)

This is used to set the state of the LED (when there's no notification showing). The first parameter is the ledmode, a second optional parameter is the hex color used for the flash and fixed color modes. This hex string must be given without a hashtag to be valid, for example: `ff0000` for red.

The ledmodes are:
|Mode|Example GIF|Description|
|-|-|-|
|`Gentle flash`|![GIF of gentle flashing LED](readme%20images/ledModeGentleFlash.gif)|Flashes the LED every second|
|`Fast flash`|![GIF of fast flashing LED](readme%20images/ledModeFastFlash.gif)|Flashes the LED every quarter of a second|
|`Fixed color`|![GIF of fixed color LED](readme%20images/ledModeFixed.gif)|Keeps the LED turned on|
|`Rainbow`|![GIF of rainbow LED mode](readme%20images/ledModeRainbow.gif)|Gently transitions through different colors|
|`Off`|![GIF of LED turned off](readme%20images/ledModeOff.gif)|Keeps the LED turned off. This is the default mode|


### Set Custom Character
![Set Custom Character Discord bot command](readme%20images/setCustomChar.png)

This interesting command allows you to save custom characters to a specific slot. There are 50 possible slots (1 - 50) in total. Using a tool like [this one](https://maxpromer.github.io/LCD-Character-Creator/) allows you to get the hex data of a custom sprite. Each row of the sprite generates a hex value such as `0x1F`. So to set a custom character, first fill in the slot, and then pass the hex values for each of the 8 rows. It's important to only use hex or decimal values since the command doesn't support binary values.

There is an optional Save parameter which lets you to save the character to the ESP's flash memory. This means that even if Disco loses power it will still store the custom characters.

The custom characters can be used using the format `((x))` where x is replaced with the slot number. This can be used in Notifications, Notes and Animation frames.


### Notification
![Notification Discord bot command](readme%20images/notification.png)

This command allows you to send a notification message to Disco. This message will temporarily interrupt the current mode until dismissed. If the message is longer than what can fit on the screen it will be split up into multiple pages. Pressing the pushbutton will skip through the pages and finally dismiss the message on the device.

The ledmode parameter is a dropdown parameter allowing you to pick a led mode to go with the notification. There's a third optional parameter which allows you to specify the ledcolor in hex. This works similar to the `/setledmode` command.

Custom characters are possible using the `((x))` format. A maximum of 7 *different* custom characters are allowed per page.

Example of a notification:

![Notification example screenshot of Disco](readme%20images/notificationExample.png)


### Set Animation
![Set Animation Discord bot command](readme%20images/setAnimation.png)

This command is used to set the frames for Animation mode. A minimum of 2 frames are required and a maximum of 24 frames are possible in total. A frame consists of maximum 32 characters. Custom characters can be used using the `((x))` format. A maximum of 7 *different* custom characters are allowed per frame, which means it's still perfectly possible to use 32 custom characters as long as the total number of different characters doesn't exceed 7 (per frame). It's also possible to use more than 7 different custom characters if they're spread over different frames.


### Set Countdown
![Set Countdown Discord bot command](readme%20images/setCountdown.png)

When in Timer & Countdown mode this command will set it to use a countdown with the set hours, minutes and seconds. It will start counting down after pressing the pushbutton.


### Set Timer
![Set Timer Discord bot command](readme%20images/setTimer.png)

When in Timer & Countdown mode this command will set it to use a timer which will count up after pressing the pushbutton. 


### Set Note
![Set Note Discord bot command](readme%20images/setNote.png)

This command will set the note, which will be visible in Note mode. There's a maximum of 32 characters possible. Custom characters are allowed with a maximum of 7 *different* ones.


### Set Stock
![Set Stock Discord bot command](readme%20images/setStock.png)

Use this command to set the stock visible in Stocks mode. Stock symbols are the only allowed input here.


### Set Object
![Set Object Discord bot command](readme%20images/setObject.png)

This command lets you change the 3D model used in 3D viewer mode.

The objdata is a simplified version of the Wavefront .obj filetype. The rules are simple:
1. Only relevant data allowed, so no comments, extra spaces or newlines
2. Only vertices(v), faces(f) and lines(l) are allowed
3. Order is important: first vertices, then faces (if any), then lines (if any)
4. Faces can only have 3 points
5. Lines can only have 2 points

There are 4 optional parameters:

|Parameter|Description|Default value|
|-|-|-|
|Pivot|The vector point in space where the object will rotate around. use a space to seperate X Y and Z values|0 0 2.5
|Offset|The offset vector to apply to the mesh (and pivot). Can be used to set the object further away from the camera|-0.5 -0.5 2|
|Zoom|A single value used to zoom the camera in or out|30|
|Rotationangle|The angle used to rotate the object every frame|10|

Example objdata for an simple 3D heart:

```
v 0 0.25 -0.25 v 0.25 0.5 -0.25 v 0.5 0.25 -0.25 v 0.5 0 -0.25 v 0 -0.5 -0.25 v -0.5 0 -0.25 v -0.5 0.25 -0.25 v -0.25 0.5 -0.25 v 0 0.25 0.25 v 0.25 0.5 0.25 v 0.5 0.25 0.25 v 0.5 0 0.25 v 0 -0.5 0.25 v -0.5 0 0.25 v -0.5 0.25 0.25 v -0.25 0.5 0.25 l 1 2 l 2 3 l 3 4 l 4 5 l 5 6 l 6 7 l 7 8 l 8 1 l 9 10 l 10 11 l 11 12 l 12 13 l 13 14 l 14 15 l 15 16 l 16 9 l 1 9 l 2 10 l 3 11 l 4 12 l 5 13 l 6 14 l 7 15 l 8 16
```

Sending this to Disco would look something like this:

![Set Object example Discord bot command](readme%20images/setObjectExample.png)



### Get IP Address 
![Get IP Discord bot command](readme%20images/getIp.png)

This will return the local IP address of the ESP.
This is useful since Disco runs a simple HTTP server allowing you to send notifications to it using a POST request. Using an app like Tasker would allow you to send phone notifications directly to Disco.

The endpoint for the POST request should be `LOCALIP/notification` (where LOCALIP is replaced with the local IP address of Disco) and the message body should have the following format:
```
{
    "message": "This is a notification ((42))",
    "ledmode": 4,
    "ledcolor": "FFFFFF" //optional
}
```
|#|LED mode|
|-|-|
|0|Rainbow|
|1|Gentle Flash|
|2|Fast Flash|
|3|Fixed Color|
|4|Off|


### Get Custom Character
![Get Custom Character Discord bot command](readme%20images/getCustomChar.png)

This is another cool command which will return the character stored at the given slot as an image(!) to Discord.
There's an optional parameter to choose the screen color (Blue or Green) of the returned image. The default color is blue. 

Example response:

![Get Custom Character Discord bot command response](readme%20images/getCustomCharResponse.png)


### Get Screen
![Get Screen Discord bot command](readme%20images/getScreen.png)

This awesome command returns a screenshot of what's currently on the display. Just as with the `/getcustomchar` command there's an optional parameter to choose the screen color.

Example response:

![Get Screen Discord bot command response](readme%20images/getScreenResponse.png)


## Set up

1. Build Disco using the above schematic.

2. Make a Discord bot following some online tutorial. You should get a Bot Token and Application ID. 

3. Make an Imgur application (Go to Imgur settings). You should get a Client ID and Client Secret. You'll also need a Refresh token. You can find more information [here](https://apidocs.imgur.com/) how to get your Refresh Token.

4. Get a free OpenWeather API Key by signing up at https://openweathermap.org/

5. Rename `Passwords.template.h` to `Passwords.h` and fill everything in. You can leave `GUILD_ID` blank.

6. Uncomment the comments-block in the `setup_commands()` function in app.ino at lines 867-879. Upload to the ESP via the Arduino IDE and this should link the slash commands with your Discord bot. Once that's done, you can comment these lines again and reupload as this only needs to happen once.

That's all! You can now enjoy your Disco desk toy.

## Used Libraries:
### [ESP Discord WebSocket Client](https://github.com/Cimera42/esp-discord-client)
Used for communicating with the bot.

### [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
Used to decode JSON payloads from Discord.