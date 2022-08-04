


function showPassword( passwordID )
{
	var x = document.getElementById( passwordID );
	
	//alert( "showPassword" );
	
	if ( x.type === "password" )
	{
		x.type = "text";
    }
	else
	{
		x.type = "password";
    }
	
	//document.getElementById( "apPasswordlabel" ).type = 
	
}

function endissubmit()
{
	if ( ( apSSID.value.length < 1 ) || ( apPassword.value.length < 8 ) )
	{
		submitbtn.disabled = true;
	}
	else
	{
		submitbtn.disabled = false;
	}

}

function apSSIDverify()
{
	endissubmit();
	
	if ( apSSID.value.length < 1 )
	{
		document.getElementById( 'apSSIDlabel' ).innerHTML = "Fill in an AP name";
	}
	else
	{
		document.getElementById( 'apSSIDlabel' ).innerHTML = "";
	}
}

function apPasswordVerify()
{
	endissubmit();
	
	if ( apPassword.value.length < 8 )
	{
		document.getElementById( 'apPasswordlabel' ).innerHTML = "Password must be at least 8 characters in length";
	}
	else
	{
		document.getElementById( 'apPasswordlabel' ).innerHTML = "";
	}

}

function apIPverify()
{
	var x = parseInt( apIP.value );
	
	if ( staIP.value == "" || ( ( x < 2 ) || ( x > 255 ) ) )
	{
		document.getElementById( 'apIPlabel' ).innerHTML = "Must be in range of 2 to 255";
		
	}
	else
	{
		document.getElementById( 'apIPlabel' ).innerHTML = "";
	}
	
}

function isNumber( evt )
{
	evt = ( evt ) ? evt : window.event;
	
	var charCode = ( evt.which ) ? evt.which : evt.keyCode;
	if ( charCode > 31 && ( charCode < 48 || charCode > 57 ) )
	{
		return false;
	}
	
	return true;
	
}

function staIPverify()
{
	var x = parseInt( staIP.value );
	
	if ( staIP.value == "" || ( ( x < 2 ) || ( x > 255 ) ) )
	{
		document.getElementById( 'staIPlabel' ).innerHTML = "Must be in range of 2 to 255";
		
	}
	else
	{
		document.getElementById( 'staIPlabel').innerHTML = "";
	}

}

function darkMode()
{
	alert( "darkMode : " );
	
	document.body.className = "dark-mode";
	
}

function lightMode()
{
	document.body.className = "light-mode";
	
}

function getDarkTheme()
{	
	if ( localStorage.darkThemeValue != null )
	{
		localStorage.darkThemeValue = ( document.getElementById( 'switchDarkThemeCtrl' ).checked == true ) ? '1' : '0';
	}
	else
		localStorage.darkThemeValue = '1';
		
	if ( localStorage.darkThemeValue == '1' )
		darkMode();
	else
		lightMode();

}

function setDarkTheme()
{	
	alert( "setDarkTheme : " + localStorage.darkThemeValue );
	
	if ( localStorage.darkThemeValue != null )
	{
		document.getElementById( 'switchDarkThemeCtrl' ).checked = ( localStorage.darkThemeValue == '1' ) ? true : false;
	}
	else
	{
		localStorage.darkThemeValue = '1';
		
		document.getElementById( 'switchDarkThemeCtrl' ).checked = true;
	}
	
	if ( localStorage.darkThemeValue == '1' )
	{	
		alert( "darkMode : " );
		
		darkMode();
	}	
	else
		lightMode();

}

function setDarkTheme_2( value )
{	
	if ( value != null )
	{
		document.getElementById( 'switchDarkThemeCtrl' ).checked = ( value == "checked" ) ? true : false;
		//document.getElementById( 'switchDarkThemeCtrl' ).value = ( value == "checked" ) ? "checked" : "unchecked";
		
		if ( value == "checked" )
		{
			darkMode();
			
			document.getElementById( "switchDarkThemeCtrl" ).value = "checked";
		}
		else
		{
			lightMode();
			
			document.getElementById( "switchDarkThemeCtrl" ).value = "unchecked";
		}			
	}
	else
	{
		document.getElementById( 'switchDarkThemeCtrl' ).checked = false;
		document.getElementById( 'switchDarkThemeCtrl' ).value = "unchecked";
		
		lightMode();
	}
		
}

function getDarkTheme_2()
{
	if ( document.getElementById( 'switchDarkThemeCtrl' ).checked == true )
	{	
		darkMode();
		
		document.getElementById( "switchDarkThemeCtrl" ).value = "checked";
		
		localStorage.darkThemeValue = '1';
	}	
	else
	{	
		lightMode();
		
		document.getElementById( "switchDarkThemeCtrl" ).value = "unchecked";
		
		localStorage.darkThemeValue = '0';
		
	}	
	
}






