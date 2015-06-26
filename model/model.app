<?xml version="1.0" encoding="UTF-8"?>
<app:application xmlns:app="http://www.sierrawireless.com/airvantage/application/1.0" type="demoDistriKit-mqtt" name="DistriKit" revision="1.0.0">
  <capabilities>
	<communication>
	  <protocol comm-id="IMEI" type="MQTT"/>
	</communication>
    <data>
      <encoding type="MQTT">
        <asset default-label="Street Light" id="st">
          <variable default-label="Light Control" path="lightstatus" type="string"/>
		  <variable default-label="Light Sensor" path="lightsensor" type="int"/>
          <variable default-label="Output Power" path="power" type="int"/>
		  <variable default-label="Failure Detected" path="failuredetected" type="boolean"/>
            <command default-label="Manual OFF" id="modeoff">                       
            </command>
			<command default-label="Mode Auto" id="modeauto">                       
            </command>
			<command default-label="Manual ON" id="modeon">                       
            </command>
        </asset>
      </encoding>
    </data>
  </capabilities>
   	<application-manager use="SOFT_IDS" />
</app:application>