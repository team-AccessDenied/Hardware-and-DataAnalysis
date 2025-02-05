USE WORKPLACE1;
SELECT E.Fname as EmployeeName, E.Lname as EmployeeLastName, S.Fname AS SupervisorFirstName,
S.Lname as SupervisorLastName


FROM EMPLOYEE E
LEFT JOIN EMPLOYEE S
ON E.Super_ssn=S.Ssn;