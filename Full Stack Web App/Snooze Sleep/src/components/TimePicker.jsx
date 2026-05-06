import React from "react";
import DatePicker from "react-datepicker";
import "react-datepicker/dist/react-datepicker.css"
import { useState } from "react"; 

function ReactDatePicker(){

    const [selectedDate, setSelectedDate] = useState(null);
    const handleDateChange = (date) => {
        setSelectedDate(date);
    };

    return(
        <DatePicker
            selected={selectedDate}
            onChange={handleDateChange}
            dateFormat="MM/dd/yyyy; hh:mm aa"
            placeholderText="Click to select a date"
            showTimeSelect
            timeInterval={30}
            timeFormat="hh:mm aa"
        />
    )
}

export default ReactDatePicker; 