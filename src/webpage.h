//============
//Webpage Code
//============
String webpageCode = R"***(
    <!DOCTYPE html>
    <head>
        <title> BIY - BREW-IT-YOURSELF </title>
        <meta http-equiv=”Content-Type” content=”text/html; charset=utf-8″>
    </head>
    <html>

    <!----------------------------CSS---------------------------->
    <style>
        body {background-color: rgba(128, 128, 128, 0.884)}
        h4 {font-family: arial; text-align: center; color: red;}
        .card {
            max-width: 450px;
            min-height: 100px;
            background: rgba(255, 0, 0, 0.521);
            padding: 10px;
            font-weight: bold;
            font: 25px calibri;
            text-align: center;
            box-sizing: border-box;
            color: blue;
            margin:20px;
            box-shadow: 0px 2px 15px 15px rgba(0,0,0,0.75);
        }
        button{
            outline: none;
            border: 2px solid blue;
            border-radius:18px;
            background-color:#FFF;
            color: blue;
            padding: 5px 25px;
            font: 15px calibri;
        }
        button:active{
            color: #fff;
            background-color:#1fa3ec;
        }
        button:hover{
            border-color:#0000ff;
        }
    </style>

    <!-------------------------JavaScrip------------------------->
    <script>
        setInterval(function() {
            getREADINGS();
        }, 1000);

        //-------------------------------------------------------
        function getREADINGS() {
            var request = new XMLHttpRequest();
            request.onreadystatechange = function() {
                if(this.readyState == 4 && this.status == 200)
                {
                    var jsonResponse = JSON.parse(this.responseText);
                    document.getElementById("VIN_1value").innerHTML =
                        jsonResponse.vin_1;
                    document.getElementById("RESIST_1value").innerHTML =
                        jsonResponse.resistencia_1;
                    document.getElementById("TEMP_1value").innerHTML =
                        jsonResponse.temp_1;
                }
            };
            request.open("GET", "readTEMP1", true);
            request.send();
        }

        //-------------------------------------------------------
        function sendButtonState(button, state) {
            console.log(button+": "+state);

            var request = new XMLHttpRequest();
            request.open("POST", "https://foo.bar/");
            request.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            request.send(encodeURI(button)+"="+state);
            }

    </script>

    <!----------------------------HTML--------------------------->
    <body onload="getREADINGS()">
        <form>
            <div class="card">
                <h1 style="color:black">Panela 1</h1>
                <h2>
                    Tens&atilde;o (V) : <span style="color:yellow" id="VIN_1value">0</span>
                </h2>
                <h2>
                    Resist&ecirc;ncia (&#8486) : <span style="color:yellow" id="RESIST_1value">0</span>
                </h2>
                <h2>
                    Temperatura (&#8451) : <span style="color:yellow" id="TEMP_1value">0</span>
                </h2>
            </div>
        </form>
        <form>
            <div class="card">
                <h1 style="color:black">Saida 1</h1>
                <h2>
                    Tens&atilde;o (V) : <span style="color:yellow" id="VIN_1value">0</span>
                </h2>
                <h2>
                    Resist&ecirc;ncia (&#8486) : <span style="color:yellow" id="RESIST_1value">0</span>
                </h2>
                <h2>
                    Temperatura (&#8451) : <span style="color:yellow" id="TEMP_1value">0</span>
                </h2>
                <p>
                    <button type="button" onclick="sendButtonState(this.id,'Manual');"">Controle Manual</button>
                    <button type="button" onclick="sendButtonState(this.id,'Automatico');">Controle Automatico</button>
                </p>
            </div>
        </form>

    </body>
</html>
)***";