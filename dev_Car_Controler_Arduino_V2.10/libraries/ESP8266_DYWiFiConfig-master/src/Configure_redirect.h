
const char PAGE_RedirectPage[] PROGMEM = R"=====(
<!DOCTYPE html><style>div.blue{border-left:6px solid #0cf;background-color:#fff}</style><body><script>function countDown(){var n=document.getElementById("timer");count>0?(count--,n.innerHTML="This page will redirect in "+count+" seconds.",setTimeout("countDown()",1e3)):window.location.href="{R-DIRECT}"}var count=15,redirect=gotoUrl</script><div class=blue>{R-MSG}<div id=timer></div></div><script>countDown()</script>
)=====";
